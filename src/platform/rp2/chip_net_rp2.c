/*
 * The network on the RP chips (chip_net.h): the CYW43 through the Pico SDK's cyw43_arch, lwIP's raw API under its lock
 * (lwIP's own task runs on the other core), the hotspot's DHCP server (dhcpserver.c) and DNS server (dnsserver.c), and
 * lwIP's mDNS responder.
 */
#include "chip_net.h"

#include <stdio.h>
#include <string.h>

#include <pico/cyw43_arch.h>

#include "dhcpserver.h"
#include "dnsserver.h"
#include "lwip/apps/mdns.h"

#define CHIP_NET_SERVICES_MAX 4

static dhcp_server_t chip_net_dhcp_server;
static dns_server_t chip_net_dns_server;
static chip_net_heard_fn chip_net_heard_call = NULL;
static const char *const *chip_net_txt = NULL;
static size_t chip_net_txt_count = 0;


static uint32_t chip_net_cyw43_auth(chip_net_security_t security) {
    switch (security) {
        case CHIP_NET_WPA_TKIP: return CYW43_AUTH_WPA_TKIP_PSK;
        case CHIP_NET_WPA2_AES: return CYW43_AUTH_WPA2_AES_PSK;
        case CHIP_NET_WPA2_MIXED: return CYW43_AUTH_WPA2_MIXED_PSK;
        case CHIP_NET_OPEN:
        default: return CYW43_AUTH_OPEN;
    }
}


bool chip_net_start(void) {
    return cyw43_arch_init() == 0;
}


void chip_net_station_up(const char *host_name) {
    if ((cyw43_state.itf_state & (1u << CYW43_ITF_STA)) != 0) {
        return;
    }
    cyw43_arch_enable_sta_mode();
    // The driver set its own default name ("PicoW", the same on every unit) as it brought the interface up; nothing
    // has asked for an address yet
    cyw43_arch_lwip_begin();
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], host_name);
    cyw43_arch_lwip_end();
}


chip_net_join_t chip_net_join(const char *ssid, const char *key, chip_net_security_t security, const uint8_t *bssid,
                              uint32_t timeout_ms) {
    int joined = bssid != NULL
                     ? cyw43_arch_wifi_connect_bssid_timeout_ms(ssid, bssid, key, chip_net_cyw43_auth(security),
                                                                timeout_ms)
                     : cyw43_arch_wifi_connect_timeout_ms(ssid, key, chip_net_cyw43_auth(security), timeout_ms);
    if (joined == PICO_OK) {
        return CHIP_NET_JOINED;
    }
    if (joined == PICO_ERROR_BADAUTH) {
        return CHIP_NET_WRONG_KEY;
    }
    return joined == PICO_ERROR_TIMEOUT ? CHIP_NET_NOT_FOUND : CHIP_NET_JOIN_FAILED;
}


bool chip_net_leave(void) {
    cyw43_arch_lwip_begin();
    int left = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();
    return left == 0;
}


bool chip_net_joined(void) {
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}


static bool chip_net_address(int itf, char *buf, size_t len) {
    const ip4_addr_t *addr = netif_ip4_addr(&cyw43_state.netif[itf]);
    if (addr == NULL || ip4_addr_isany(addr)) {
        return false;
    }
    return ip4addr_ntoa_r(addr, buf, (int) len) != NULL;
}


bool chip_net_station_address(char *buf, size_t len) {
    return chip_net_address(CYW43_ITF_STA, buf, len);
}


bool chip_net_hotspot_address(char *buf, size_t len) {
    return chip_net_address(CYW43_ITF_AP, buf, len);
}


bool chip_net_rssi(int32_t *rssi) {
    cyw43_arch_lwip_begin();
    bool read = cyw43_wifi_get_rssi(&cyw43_state, rssi) == 0;
    cyw43_arch_lwip_end();
    return read;
}


/*
 * A scan's result as the interface gives it. The radio tells WPA2 from WPA but not AES from TKIP: WPA2 alone is taken
 * as WPA2 with AES, WPA2 and WPA as mixed; a security with neither, but not open, is WEP alone
 */
static int chip_net_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    (void) env;
    chip_net_heard_t heard = {
        .ssid = result->ssid,
        .ssid_len = result->ssid_len,
        .rssi = result->rssi,
        .channel = (uint8_t) (result->channel & 0xff),
        .security = (result->auth_mode & 4) != 0 ? ((result->auth_mode & 2) != 0 ? CHIP_NET_WPA2_MIXED : CHIP_NET_WPA2_AES)
                                                 : ((result->auth_mode & 2) != 0 ? CHIP_NET_WPA_TKIP : CHIP_NET_OPEN),
        .joinable = (result->auth_mode & 6) != 0 || result->auth_mode == 0,
    };
    memcpy(heard.bssid, result->bssid, sizeof(heard.bssid));
    if (chip_net_heard_call != NULL) {
        chip_net_heard_call(&heard);
    }
    return 0;
}


bool chip_net_scan_start(chip_net_heard_fn heard) {
    chip_net_heard_call = heard;
    cyw43_wifi_scan_options_t options = {0};
    cyw43_arch_lwip_begin();
    int err = cyw43_wifi_scan(&cyw43_state, &options, NULL, chip_net_scan_result);
    cyw43_arch_lwip_end();
    if (err != 0) {
        printf("Wi-Fi: a scan did not start (%d)\n", err);
    }
    return err == 0;
}


bool chip_net_scanning(void) {
    return cyw43_wifi_scan_active(&cyw43_state);
}


bool chip_net_hotspot_start(const char *name, const char *key, uint8_t channel, const char *address) {
    if (channel != 0) {
        cyw43_wifi_ap_set_channel(&cyw43_state, channel);
    }
    cyw43_arch_enable_ap_mode(name, key, CYW43_AUTH_WPA2_AES_PSK);

    ip_addr_t gw;
    ip4_addr_t mask;
    ip4addr_aton(address, ip_2_ip4(&gw));
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // The DHCP and DNS servers are lwIP's raw API: under its lock, as lwIP's own task runs on the other core (review D3)
    cyw43_arch_lwip_begin();
    dhcp_server_init(&chip_net_dhcp_server, &gw, &mask);
    // The server this file keeps. A second, declared where the hotspot started, was handed over instead, and was gone
    // when that function returned: every query then read memory that was no longer its own (ADR-012; audit W1)
    dns_server_init(&chip_net_dns_server, &gw);
    cyw43_arch_lwip_end();
    return true;
}


void chip_net_hotspot_stop(void) {
    // Under lwIP's lock: a query may be answered on the other core while its server is removed (review D3)
    cyw43_arch_lwip_begin();
    dhcp_server_deinit(&chip_net_dhcp_server);
    dns_server_deinit(&chip_net_dns_server);
    cyw43_arch_lwip_end();
    cyw43_arch_disable_ap_mode();
}


uint32_t chip_net_hotspot_addresses_given(void) {
    return chip_net_dhcp_server.given;
}


static void chip_net_service_txt(struct mdns_service *service, void *txt_userdata) {
    (void) txt_userdata;
    for (size_t idx = 0; idx < chip_net_txt_count; idx += 1) {
        size_t len = strlen(chip_net_txt[idx]);
        if (len == 0 || len > 255 || mdns_resp_add_service_txtitem(service, chip_net_txt[idx], (u8_t) len) != ERR_OK) {
            printf("mdns add service txt failed\n");
            return;
        }
    }
}


void chip_net_announce(const char *host_name, const chip_net_service_t *services, size_t service_count,
                       const char *const *txt, size_t txt_count) {
    chip_net_txt = txt;
    chip_net_txt_count = txt_count;
    struct netif *station = &cyw43_state.netif[CYW43_ITF_STA];
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    mdns_resp_add_netif(station, host_name);
    for (size_t idx = 0; idx < service_count && idx < CHIP_NET_SERVICES_MAX; idx += 1) {
        mdns_resp_add_service(station, services[idx].instance, services[idx].type, DNSSD_PROTO_TCP, services[idx].port,
                              chip_net_service_txt, NULL);
    }
    cyw43_arch_lwip_end();
}


void chip_net_led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}
