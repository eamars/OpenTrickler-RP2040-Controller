#include <pico/cyw43_arch.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "wireless.h"
#include "eeprom.h"
#include "unit_id.h"
#include "access_point_mode.h"
#include "rest_endpoints.h"
#include "http_server.h"
#include "common.h"
#include "settings.h"
#include "version.h"
#include "update.h"
#include "update_poll.h"
#include "lwip/apps/mdns.h"
#include "events.h"
#include "setup.h"
#include <string.h>
#include <strings.h>

#define WIRELESS_REJOIN_MS 60000            // a station set up and not joined tries again this often (ADR-004) p
#define WIRELESS_REJOIN_TIMEOUT_MS 15000    // each try's time to join

static void wireless_station_mdns(void);
static void wireless_rejoin(void);
static void wireless_station_up(void);
static void wireless_hotspot_on_strongest_channel(void);
static void wireless_scan_run(void);
static void wireless_join_run(void);
static void srv_txt(struct mdns_service *service, void *txt_userdata);


#define LED_INTERFACE_MINIMUM_POLL_PERIOD_MS    20

// The hostname is used by STA mode to advertise mDNS. The value is also used for AP mode as the SSID
char host_name[18];

typedef enum {
    WIRELESS_STATE_NOT_INITIALIZED = 0,
    WIRELESS_STATE_IDLE,
    WIRELESS_STATE_AP_MODE_INIT,
    WIRELESS_STATE_AP_MODE_LISTEN,
    WIRELESS_STATE_STA_MODE_INIT,
    WIRELESS_STATE_STA_MODE_LISTEN,
} wireless_state_t;

typedef enum {
    WIRELESS_CTRL_NOP = 0,
    WIRELESS_CTRL_CYW43_INIT,
    WIRELESS_CTRL_CYW43_DEINIT,
    WIRELESS_CTRL_DISCONNECT,
    WRIELESS_CTRL_START_AP_MODE,
    WRIELESS_CTRL_START_STA_MODE,
    WIRELESS_CTRL_LED_ON,
    WIRELESS_CTRL_LED_OFF,
    WIRELESS_CTRL_SCAN,         // the set-up's scan (ADR-008, gate 4)
    WIRELESS_CTRL_JOIN,         // the set-up's join, the hotspot kept up
    WIRELESS_CTRL_SETUP_END,    // the set-up done: the hotspot stops once the reply is out
} wireless_ctrl_t;


typedef struct {
    eeprom_wireless_metadata_t eeprom_wireless_metadata;
    wireless_state_t current_wireless_state;
} wireless_config_t;


static wireless_config_t wireless_config;
const eeprom_wireless_metadata_t default_eeprom_wireless_metadata = {
    .wireless_data_rev = 0,
    .ssid = "",
    .pw = "",
    .auth = AUTH_WPA2_MIXED_PSK,
    .timeout_ms = 30000,    // 30s
    .enable = false,
};

static QueueHandle_t wireless_ctrl_queue;

// Render task
const char * wireless_state_strings[] = {
    "Not Initialized",
    "Idling",
    "AP Mode Init",
    "AP Mode Listen",
    "STA Mode Init",
    "STA Mode Listen"
};




/*
    The Wi-Fi's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/wifi). Each applies at the
    next start, when the unit joins the network. The key is written only: never in a reply, nor in the export.
*/
static const setting_choice_t wifi_auth_choices[] = {
    {.held = AUTH_OPEN, .text = "open", .label = "Open"},
    {.held = AUTH_WPA_TKIP_PSK, .text = "wpa_tkip", .label = "WPA (TKIP)"},
    {.held = AUTH_WPA2_AES_PSK, .text = "wpa2_aes", .label = "WPA2 (AES)"},
    {.held = AUTH_WPA2_MIXED_PSK, .text = "wpa2_mixed", .label = "WPA/WPA2 mixed"},
};


#define WIFI_FIELD(member) SETTING_AT(eeprom_wireless_metadata_t, member)

static const setting_field_t wifi_fields[] = {
    {.name = "enable", .label = "Join a network", .short_label = "Wi-Fi",
     .about = "The unit joins this network at its start; off, it is its own access point",
     .type = SETTING_BOOLEAN, WIFI_FIELD(enable), .applies = "at_restart"},
    {.name = "ssid", .label = "Network name",
     .about = "The network's name (SSID)",
     .type = SETTING_STRING, WIFI_FIELD(ssid), .length_min = 1, .applies = "at_restart"},
    {.name = "password", .label = "Password",
     .about = "The network's password; never sent back",
     .type = SETTING_STRING, WIFI_FIELD(pw), .access = SETTING_WRITE_ONLY, .applies = "at_restart"},
    {.name = "auth", .label = "Security",
     .about = "The network's security",
     .type = SETTING_CHOICE, WIFI_FIELD(auth), SETTING_CHOICES(wifi_auth_choices), .applies = "at_restart"},
    {.name = "timeout_ms", .label = "Join timeout",
     .about = "How long the unit tries to join before it gives up",
     .type = SETTING_INTEGER, WIFI_FIELD(timeout_ms), .unit = "ms", .min = 1000.0f, .max = 120000.0f,
     .applies = "at_restart"},
};


static void *wifi_settings_live(int32_t id) {
    (void) id;
    return &wireless_config.eeprom_wireless_metadata;
}


static bool wifi_settings_is_saved(void) {
    return config_is_saved(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata,
                           sizeof(eeprom_wireless_metadata_t));
}


static const settings_resource_t wifi_settings = {
    .path = "/api/v1/settings/wifi",
    .save_path = "/api/v1/settings/wifi/save",
    .revert_path = "/api/v1/settings/wifi/revert",
    .addr = EEPROM_WIRELESS_CONFIG_BASE_ADDR,
    .label = "Wi-Fi",
    .about = "The network the unit joins",
    SETTING_FIELDS(wifi_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(eeprom_wireless_metadata_t),
    .live = wifi_settings_live,
    .save = wireless_config_save,
    .is_saved = wifi_settings_is_saved,
};


bool wireless_init() {
    bool is_ok = true;

    memset(&wireless_config, 0x00, sizeof(wireless_config_t));
    is_ok = load_config(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata, &default_eeprom_wireless_metadata, sizeof(wireless_config.eeprom_wireless_metadata), EEPROM_WIRELESS_CONFIG_METADATA_REV);
    if (!is_ok) {
        printf("Unable to read WiFi configuration\n");
        return false;
    }

    /*
        The name and the key are used as strings everywhere, and are stored in 32 and 64 bytes: a name of 32
        characters or a key of 64 had no ending, and what read them ran on into what lies behind (ADR-012; audit W2).
        One stored that long before this is cut by its last character here; it must be entered again.
    */
    wireless_config.eeprom_wireless_metadata.ssid[sizeof(wireless_config.eeprom_wireless_metadata.ssid) - 1] = '\0';
    wireless_config.eeprom_wireless_metadata.pw[sizeof(wireless_config.eeprom_wireless_metadata.pw) - 1] = '\0';

    // Create Wireless handler task
    // Its stack holds the set-up's join's store too, a settings write with two copies of the struct (ADR-008, gate 4:
    // it took 1,640 bytes at most, measured with 1,024 words); `stats tasks` gives what it never touched
    xTaskCreate(wireless_task, "Wireless Task", 768, NULL, 3, NULL);

    // Register to eeprom save all
    eeprom_register_handler(wireless_config_save);
    settings_register(&wifi_settings);

    // The host name: the id's first four characters (ADR-003, gate 4), the id the Pico's own (ADR-008, gate 4)
    char id[UNIT_ID_LENGTH + 1];
    unit_id_get(id, sizeof(id));
    snprintf(host_name, sizeof(host_name), "opentrickler-%.4s", id);
    // And the access point's key, the unit's own (ADR-008, gate 4)
    access_point_mode_init();


    return is_ok;
}


uint32_t wireless_join_timeout_ms(void) {
    return wireless_config.eeprom_wireless_metadata.timeout_ms;
}


bool wireless_config_save() {
    bool is_ok = save_config(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
    return is_ok;
}


uint32_t get_cyw43_auth(cyw43_auth_t auth) {
    uint32_t cyw43_auth = 0;

    switch (auth) {
        case AUTH_OPEN:
            cyw43_auth = CYW43_AUTH_OPEN;
            break;
        case AUTH_WPA_TKIP_PSK:
            cyw43_auth = CYW43_AUTH_WPA_TKIP_PSK;
            break;
        case AUTH_WPA2_AES_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_AES_PSK;
            break;
        case AUTH_WPA2_MIXED_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_MIXED_PSK;
            break;
        default:
            break;
    }

    return cyw43_auth;
}


void led_interface_task(void *p) {
    wireless_ctrl_t wireless_ctrl;
    uint32_t blink_interval_ms = 0;
    bool prev_led_state = false;
    bool led_state = prev_led_state;

    while (true) {
        prev_led_state = led_state;

        switch (wireless_config.current_wireless_state) {
            case WIRELESS_STATE_NOT_INITIALIZED:
            case WIRELESS_STATE_IDLE:
                blink_interval_ms = 0;
                led_state = false;
                break;
            case WIRELESS_STATE_AP_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_AP_MODE_LISTEN:
                led_state = !led_state;
                blink_interval_ms = 1000;
                break;
            case WIRELESS_STATE_STA_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_STA_MODE_LISTEN:
                led_state = true;
                blink_interval_ms = 0;
                break;
            default:
                break;
        }

        if (led_state != prev_led_state) {
            if (led_state) {
                wireless_ctrl = WIRELESS_CTRL_LED_ON;
            }
            else {
                wireless_ctrl = WIRELESS_CTRL_LED_OFF;
            }
            xQueueSend(wireless_ctrl_queue, &wireless_ctrl, 0);
        }

        
        vTaskDelay(pdMS_TO_TICKS(MAX(blink_interval_ms, LED_INTERFACE_MINIMUM_POLL_PERIOD_MS)));
    }
}


// The unit's address on the network it is on: its station's, or its own access point's
static bool wireless_own_address(char *buf, size_t len) {
    int itf = wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN ? CYW43_ITF_AP : CYW43_ITF_STA;
    const ip4_addr_t *addr = netif_ip4_addr(&cyw43_state.netif[itf]);
    if (addr == NULL || ip4_addr_isany(addr)) {
        return false;
    }
    return ip4addr_ntoa_r(addr, buf, (int) len) != NULL;
}


/*
 * The Wi-Fi settings the unit started with. A write to them applies at the restart, as their table says: the retry and
 * the trial's confirmation read this copy, never a write not yet saved, nor one half made (review C2)
 */
static eeprom_wireless_metadata_t wireless_started_with;
// The station's last join, as the SDK answered it (PICO_OK when joined): the screen says why it failed, as the old
// Wi-Fi screen gave the link's status (ADR-010, gate 6)
static volatile int wireless_last_join = PICO_OK;


// mDNS on the station, once: the unit's name, and its two services
static void wireless_station_mdns(void) {
    static bool announced = false;
    if (announced) {
        return;
    }
    announced = true;
    cyw43_arch_lwip_begin();
    mdns_resp_init();

    // The opentrickler can be accessed via hostname.local
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], host_name);

    // Add service HTTP, allowing user to access the web interface with hostname.local format
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "rest_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);

    // Add secondary service to allow client to discover with service _opentrickler._tcp.local
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "app_httpd", "_opentrickler", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    cyw43_arch_lwip_end();
}


// --- The station's state, the networks heard and the set-up's join (ADR-008, gate 4)

#define WIRELESS_NETWORKS_MAX 24
#define WIRELESS_ACCESS_POINTS_MAX 32   // the access points behind them: a mesh has a few under one name
#define WIRELESS_SCAN_PASSES 2          // each scan hears part of the networks (gate 1's probe: 7, then 10, of ~18)
#define WIRELESS_SCAN_PASS_MS 4500

typedef enum {
    STATION_IDLE = 0,
    STATION_JOINING,
    STATION_JOINED,
    STATION_FAILED,
} station_state_t;

// The station's last join, whichever asked it; read and written in critical sections, as the API's task reads it
static station_state_t wireless_station_state = STATION_IDLE;
static char wireless_station_ssid[33];
static const char *wireless_station_reason = NULL;
static TickType_t wireless_station_since = 0;           // when the state was last set: a join's start

// A join asked, waiting for the Wi-Fi task: its network, its key (wiped once tried), its security, who asked
static struct {
    char ssid[33];
    char key[64];
    cyw43_auth_t auth;
    bool stored_key;        // the stored network's own key: not written again
    activity_by_t by;
} wireless_join_asked;

// The networks heard by the last scan, merged by name, in the order heard, and the order a reader takes them in,
// strongest first; and each access point heard, its network by index: from the FreeRTOS heap at the first scan (RAM
// at link is full)
typedef struct {
    uint8_t bssid[6];
    uint8_t channel;
    uint8_t network;        // its index in wireless_heard
    int16_t rssi;
} wireless_access_point_t;

static wireless_network_t *wireless_heard = NULL;
static uint8_t wireless_heard_order[WIRELESS_NETWORKS_MAX];
static uint8_t wireless_heard_count = 0;
static wireless_access_point_t *wireless_access_points = NULL;
static uint8_t wireless_access_point_count = 0;
static volatile bool wireless_scan_running = false;

/*
 * The hotspot's channel (ADR-008, gate 4's design): the CYW43 keeps its hotspot and its station on one channel, so a
 * join to a router on another moves the hotspot, and a phone on it may not follow (the owner's iPhone did not). The
 * hotspot starts on the strongest access point's channel, most often the person's own router, and a join takes its
 * network's access point on that channel when one was heard. 0: not known (it moved with a join that could not)
 */
static uint8_t wireless_hotspot_channel = 0;


static void wireless_station_set(station_state_t state, const char *ssid, const char *reason) {
    size_t len = ssid != NULL ? strnlen(ssid, sizeof(wireless_station_ssid) - 1) : 0;
    taskENTER_CRITICAL();
    wireless_station_state = state;
    if (ssid != NULL) {
        memcpy(wireless_station_ssid, ssid, len);
        wireless_station_ssid[len] = '\0';
    }
    wireless_station_reason = reason;
    wireless_station_since = xTaskGetTickCount();
    taskEXIT_CRITICAL();
}


// A join's end in the API's words
static const char *wireless_join_reason(int joined) {
    if (joined == PICO_ERROR_BADAUTH) {
        return "wrong_password";
    }
    return joined == PICO_ERROR_TIMEOUT ? "timeout" : "failed";
}


// The station's interface up, beside the hotspot if it is up: a unit that started with Wi-Fi off never brought it up
static void wireless_station_up(void) {
    if ((cyw43_state.itf_state & (1u << CYW43_ITF_STA)) != 0) {
        return;
    }
    cyw43_arch_enable_sta_mode();
    // The name the router is told by DHCP: the unit's own, as mDNS announces it. The driver set its own default
    // ("PicoW", the same on every unit) as it brought the interface up; nothing has asked for an address yet
    cyw43_arch_lwip_begin();
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], host_name);
    cyw43_arch_lwip_end();
}


void wireless_get_station(wireless_station_info_t *station) {
    static const char *const names[] = {"idle", "joining", "joined", "failed"};
    memset(station, 0, sizeof(*station));
    taskENTER_CRITICAL();
    station_state_t state = wireless_station_state;
    memcpy(station->ssid, wireless_station_ssid, sizeof(station->ssid));
    station->reason = wireless_station_reason;
    TickType_t since = wireless_station_since;
    taskEXIT_CRITICAL();
    if (state == STATION_JOINING) {
        station->joining_ms = (uint32_t) ((xTaskGetTickCount() - since) * portTICK_PERIOD_MS);
    }
    bool up = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
    if (state == STATION_JOINED && !up) {
        state = STATION_IDLE;       // it left (its router went away, `wifi leave`): the retry says what comes next
    }
    station->state = names[state];
    const ip4_addr_t *addr = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
    if (up && addr != NULL && !ip4_addr_isany(addr)) {
        ip4addr_ntoa_r(addr, station->address, sizeof(station->address));
    }
}


// The settings' word for a security, and the security for a word
static const char *wireless_auth_text(cyw43_auth_t auth) {
    for (size_t idx = 0; idx < sizeof(wifi_auth_choices) / sizeof(wifi_auth_choices[0]); idx += 1) {
        if (wifi_auth_choices[idx].held == auth) {
            return wifi_auth_choices[idx].text;
        }
    }
    return "wpa2_mixed";
}


static bool wireless_auth_from_text(const char *text, cyw43_auth_t *auth) {
    for (size_t idx = 0; idx < sizeof(wifi_auth_choices) / sizeof(wifi_auth_choices[0]); idx += 1) {
        if (strcmp(wifi_auth_choices[idx].text, text) == 0) {
            *auth = (cyw43_auth_t) wifi_auth_choices[idx].held;
            return true;
        }
    }
    return false;
}


/*
 * A scan's network, merged by name into what was heard: its strongest signal and that one's security. A hidden
 * network (no name) is left out, and one with WEP alone, which the unit cannot join. The radio tells WPA2 from WPA
 * but not AES from TKIP: WPA2 alone is taken as wpa2_aes, WPA2 and WPA as wpa2_mixed
 */
static int wireless_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    (void) env;
    if (result->ssid_len == 0 || result->ssid_len > 32 || ((result->auth_mode & 6) == 0 && result->auth_mode != 0)) {
        return 0;
    }
    cyw43_auth_t auth = (result->auth_mode & 4) != 0 ? ((result->auth_mode & 2) != 0 ? AUTH_WPA2_MIXED_PSK
                                                                                      : AUTH_WPA2_AES_PSK)
                                                     : ((result->auth_mode & 2) != 0 ? AUTH_WPA_TKIP_PSK : AUTH_OPEN);
    taskENTER_CRITICAL();
    uint8_t idx = 0;
    while (idx < wireless_heard_count && (strlen(wireless_heard[idx].ssid) != result->ssid_len ||
                                          memcmp(wireless_heard[idx].ssid, result->ssid, result->ssid_len) != 0)) {
        idx += 1;
    }
    if (idx == wireless_heard_count && idx < WIRELESS_NETWORKS_MAX) {
        memcpy(wireless_heard[idx].ssid, result->ssid, result->ssid_len);
        wireless_heard[idx].ssid[result->ssid_len] = '\0';
        wireless_heard[idx].rssi = INT16_MIN;
        wireless_heard_count += 1;
    }
    if (idx < wireless_heard_count && result->rssi > wireless_heard[idx].rssi) {
        wireless_heard[idx].rssi = result->rssi;
        wireless_heard[idx].auth = wireless_auth_text(auth);
    }
    // Its access point, by BSSID, its strongest signal
    if (idx < wireless_heard_count) {
        uint8_t ap = 0;
        while (ap < wireless_access_point_count && memcmp(wireless_access_points[ap].bssid, result->bssid, 6) != 0) {
            ap += 1;
        }
        if (ap == wireless_access_point_count && ap < WIRELESS_ACCESS_POINTS_MAX) {
            memcpy(wireless_access_points[ap].bssid, result->bssid, 6);
            wireless_access_points[ap].rssi = INT16_MIN;
            wireless_access_point_count += 1;
        }
        if (ap < wireless_access_point_count && result->rssi > wireless_access_points[ap].rssi) {
            wireless_access_points[ap].rssi = result->rssi;
            wireless_access_points[ap].channel = (uint8_t) (result->channel & 0xff);
            wireless_access_points[ap].network = idx;
        }
    }
    taskEXIT_CRITICAL();
    return 0;
}


// The strongest access point heard, of a network (NULL: any), on a channel (0: any); NULL when none was
static const wireless_access_point_t *wireless_access_point_of(const char *ssid, uint8_t channel) {
    const wireless_access_point_t *best = NULL;
    for (uint8_t ap = 0; ap < wireless_access_point_count; ap += 1) {
        const wireless_access_point_t *one = &wireless_access_points[ap];
        if ((ssid == NULL || strcmp(wireless_heard[one->network].ssid, ssid) == 0) &&
            (channel == 0 || one->channel == channel) && (best == NULL || one->rssi > best->rssi)) {
            best = one;
        }
    }
    return best;
}


// In the Wi-Fi task: two scans, merged, what was heard before forgotten
static void wireless_scan_run(void) {
    if (wireless_heard == NULL) {
        wireless_heard = pvPortMalloc(WIRELESS_NETWORKS_MAX * sizeof(wireless_network_t));
        wireless_access_points = pvPortMalloc(WIRELESS_ACCESS_POINTS_MAX * sizeof(wireless_access_point_t));
    }
    if (wireless_heard == NULL || wireless_access_points == NULL) {
        printf("Wi-Fi: no room for the networks heard\n");
        wireless_scan_running = false;
        return;
    }
    taskENTER_CRITICAL();
    wireless_heard_count = 0;
    wireless_access_point_count = 0;
    taskEXIT_CRITICAL();
    wireless_station_up();
    TickType_t started = xTaskGetTickCount();
    for (int pass = 0; pass < WIRELESS_SCAN_PASSES; pass += 1) {
        cyw43_wifi_scan_options_t options = {0};
        cyw43_arch_lwip_begin();
        int err = cyw43_wifi_scan(&cyw43_state, &options, NULL, wireless_scan_result);
        cyw43_arch_lwip_end();
        if (err != 0) {
            printf("Wi-Fi: a scan did not start (%d)\n", err);
            break;
        }
        TickType_t pass_started = xTaskGetTickCount();
        while (cyw43_wifi_scan_active(&cyw43_state) &&
               xTaskGetTickCount() - pass_started < pdMS_TO_TICKS(WIRELESS_SCAN_PASS_MS)) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    // Strongest first, once, so a reader takes them in order a network at a time; the table itself stays as heard,
    // the access points pointing into it
    taskENTER_CRITICAL();
    for (uint8_t idx = 0; idx < wireless_heard_count; idx += 1) {
        uint8_t at = idx;
        while (at > 0 && wireless_heard[wireless_heard_order[at - 1]].rssi < wireless_heard[idx].rssi) {
            wireless_heard_order[at] = wireless_heard_order[at - 1];
            at -= 1;
        }
        wireless_heard_order[at] = idx;
    }
    taskEXIT_CRITICAL();
    wireless_scan_running = false;
    printf("Wi-Fi: scanned, %u networks heard in %lu ms\n", (unsigned) wireless_heard_count,
           (unsigned long) ((xTaskGetTickCount() - started) * portTICK_PERIOD_MS));
}


bool wireless_scan_start(void) {
    taskENTER_CRITICAL();
    bool free = !wireless_scan_running && wireless_station_state != STATION_JOINING;
    if (free) {
        wireless_scan_running = true;
    }
    taskEXIT_CRITICAL();
    if (!free) {
        return false;
    }
    wireless_ctrl_t ctrl = WIRELESS_CTRL_SCAN;
    if (wireless_ctrl_queue == NULL || xQueueSend(wireless_ctrl_queue, &ctrl, 0) != pdTRUE) {
        wireless_scan_running = false;
        return false;
    }
    return true;
}


bool wireless_scanning(void) {
    return wireless_scan_running;
}


bool wireless_network_at(size_t idx, wireless_network_t *out) {
    taskENTER_CRITICAL();
    bool there = wireless_heard != NULL && idx < wireless_heard_count && !wireless_scan_running;
    if (there) {
        *out = wireless_heard[wireless_heard_order[idx]];
    }
    taskEXIT_CRITICAL();
    return there;
}


// A join taken: asked of the Wi-Fi task, the station joining from now, so a read straight after says so
static wireless_join_result_t wireless_join_ask(const char *ssid, const char *key, cyw43_auth_t auth, bool stored_key,
                                                activity_by_t by) {
    if (wireless_config.current_wireless_state != WIRELESS_STATE_AP_MODE_LISTEN) {
        return WIRELESS_JOIN_NOT_HOTSPOT;
    }
    taskENTER_CRITICAL();
    bool free = !wireless_scan_running && wireless_station_state != STATION_JOINING;
    if (free) {
        wireless_station_state = STATION_JOINING;
        wireless_station_reason = NULL;
    }
    taskEXIT_CRITICAL();
    if (!free) {
        return WIRELESS_JOIN_BUSY;
    }
    snprintf(wireless_join_asked.ssid, sizeof(wireless_join_asked.ssid), "%s", ssid);
    snprintf(wireless_join_asked.key, sizeof(wireless_join_asked.key), "%s", key);
    wireless_join_asked.auth = auth;
    wireless_join_asked.stored_key = stored_key;
    wireless_join_asked.by = by;
    wireless_station_set(STATION_JOINING, ssid, NULL);
    wireless_ctrl_t ctrl = WIRELESS_CTRL_JOIN;
    if (xQueueSend(wireless_ctrl_queue, &ctrl, 0) != pdTRUE) {
        wireless_station_set(STATION_IDLE, "", NULL);
        return WIRELESS_JOIN_BUSY;
    }
    return WIRELESS_JOIN_TAKEN;
}


wireless_join_result_t wireless_join(const char *ssid, const char *key, const char *auth_text, activity_by_t by) {
    cyw43_auth_t auth;
    size_t ssid_len = strlen(ssid);
    if (ssid_len == 0 || ssid_len >= sizeof(wireless_join_asked.ssid) - 1) {
        return WIRELESS_JOIN_BAD_SSID;
    }
    if (!wireless_auth_from_text(auth_text, &auth)) {
        return WIRELESS_JOIN_BAD_AUTH;
    }
    size_t key_len = strlen(key);
    if (auth != AUTH_OPEN && (key_len < 8 || key_len >= sizeof(wireless_join_asked.key))) {
        return WIRELESS_JOIN_BAD_KEY;
    }
    return wireless_join_ask(ssid, auth != AUTH_OPEN ? key : "", auth, false, by);
}


wireless_join_result_t wireless_join_stored(activity_by_t by) {
    if (wireless_started_with.ssid[0] == '\0') {
        return WIRELESS_JOIN_BAD_SSID;
    }
    return wireless_join_ask(wireless_started_with.ssid, wireless_started_with.pw, wireless_started_with.auth, true,
                             by);
}


/*
 * In the Wi-Fi task: the join, the hotspot kept up. Joined: the network stored, by whom it was asked, and in effect
 * now, as if the unit had started with it (no restart waits); announced by mDNS on it. Refused: the station leaves
 * the network, so it stops trying, and nothing is stored. The key is wiped either way
 */
static void wireless_join_run(void) {
    const char *ssid = wireless_join_asked.ssid;
    cyw43_auth_t auth = wireless_join_asked.auth;
    wireless_station_up();
    event_t began = {.source = EVENT_SOURCE_WIFI, .kind = EVENT_JOINING, .by = (uint8_t) wireless_join_asked.by};
    events_add(&began);
    const char *password = auth != AUTH_OPEN ? wireless_join_asked.key : NULL;
    const wireless_access_point_t *on_channel = wireless_hotspot_channel != 0 && wireless_access_points != NULL
                                                    ? wireless_access_point_of(ssid, wireless_hotspot_channel) : NULL;
    int joined;
    if (on_channel != NULL) {
        printf("Wi-Fi: joining %s on channel %u, the hotspot's, the hotspot kept up\n", ssid, wireless_hotspot_channel);
        joined = cyw43_arch_wifi_connect_bssid_timeout_ms(ssid, on_channel->bssid, password, get_cyw43_auth(auth),
                                                         wireless_join_timeout_ms());
    }
    else {
        printf("Wi-Fi: joining %s, heard on no access point on channel %u: the hotspot moves to its channel\n", ssid,
               wireless_hotspot_channel);
        joined = cyw43_arch_wifi_connect_timeout_ms(ssid, password, get_cyw43_auth(auth), wireless_join_timeout_ms());
        wireless_hotspot_channel = 0;   // where it went is not known here
    }
    if (joined != PICO_OK) {
        cyw43_arch_lwip_begin();
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        cyw43_arch_lwip_end();
        memset(wireless_join_asked.key, 0, sizeof(wireless_join_asked.key));
        wireless_station_set(STATION_FAILED, ssid, wireless_join_reason(joined));
        event_t ended = {.source = EVENT_SOURCE_WIFI, .kind = EVENT_NOT_JOINED, .by = (uint8_t) wireless_join_asked.by,
                         .reason = (uint8_t) (joined == PICO_ERROR_BADAUTH ? EVENT_REASON_WRONG_PASSWORD :
                                              joined == PICO_ERROR_TIMEOUT ? EVENT_REASON_TIMEOUT : EVENT_REASON_FAILED)};
        events_add(&ended);
        printf("Wi-Fi: not joined %s (%d), nothing stored\n", ssid, joined);
        return;
    }
    static const char *const names[] = {"enable", "ssid", "auth", "password"};
    const char *values[] = {"true", ssid, wireless_auth_text(auth), wireless_join_asked.key};
    const char *why = settings_write_and_store_by("settings/wifi", names, values,
                                                  wireless_join_asked.stored_key ? 3 : 4, wireless_join_asked.by);
    memset(wireless_join_asked.key, 0, sizeof(wireless_join_asked.key));
    if (why != NULL) {
        printf("Wi-Fi: joined %s, not stored: %s\n", ssid, why);
    }
    settings_in_effect("settings/wifi");
    wireless_started_with = wireless_config.eeprom_wireless_metadata;
    wireless_station_set(STATION_JOINED, ssid, NULL);
    event_t ended = {.source = EVENT_SOURCE_WIFI, .kind = EVENT_JOINED, .by = (uint8_t) wireless_join_asked.by};
    events_add(&ended);
    wireless_station_mdns();
    printf("Wi-Fi: joined %s, stored; the hotspot stays up\n", ssid);
}


/*
 * A station set up and not joined tries again (ADR-004; handed on by ADR-012): the unit that started before its
 * router, as its own access point, and the one whose router went away. The access point stays up while it tries, so a
 * person can still reach the unit; once the station joins, it stops, and the root page is the portal again
 */
/*
 * Before the hotspot starts: a scan (some 1.3 s; its networks ready for the set-up's list), and the hotspot's channel
 * the strongest access point's, which is most often the person's own router
 */
static void wireless_hotspot_on_strongest_channel(void) {
    wireless_scan_running = true;
    wireless_scan_run();
    const wireless_access_point_t *strongest = wireless_access_points != NULL
                                                   ? wireless_access_point_of(NULL, 0) : NULL;
    wireless_hotspot_channel = strongest != NULL && strongest->channel >= 1 && strongest->channel <= 13
                                   ? strongest->channel : 0;
    if (wireless_hotspot_channel != 0) {
        cyw43_wifi_ap_set_channel(&cyw43_state, wireless_hotspot_channel);
        printf("Wi-Fi: the hotspot on channel %u, the strongest network's (%s, %d dBm)\n", wireless_hotspot_channel,
               wireless_heard[strongest->network].ssid, strongest->rssi);
    }
}


// The hotspot stopped, the station joined: the unit on its network alone, where every page is the portal
static void wireless_hotspot_stop(void) {
    if (wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN) {
        access_point_mode_stop();
        cyw43_arch_disable_ap_mode();
    }
    wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
}


/*
 * The set-up's end (ADR-008, gate 4): finished or skipped, remembered. Joined, the hotspot stops, a moment after the
 * reply has gone out over it, and the unit polls for an update as a station does at its start; not joined, the
 * hotspot stays, the unit's only network
 */
#define WIRELESS_SETUP_END_MS 2000

bool wireless_set_up(void) {
    // A network named counts: a unit set up before the set-up existed, by the wizard or the settings
    return wireless_config.eeprom_wireless_metadata.setup_done || wireless_config.eeprom_wireless_metadata.ssid[0] != '\0';
}


const char *wireless_setup_done(activity_by_t by, bool *hotspot_stays) {
    wireless_config.eeprom_wireless_metadata.setup_done = true;
    const char *why = settings_write_and_store_by("settings/wifi", NULL, NULL, 0, by);
    if (why != NULL) {
        return why;
    }
    setup_set_step(SETUP_STEP_DONE, NULL, by);
    bool hotspot = wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN;
    bool joined = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
    *hotspot_stays = hotspot && !joined;
    if (hotspot && joined) {
        wireless_ctrl_t ctrl = WIRELESS_CTRL_SETUP_END;
        xQueueSend(wireless_ctrl_queue, &ctrl, 0);
    }
    return NULL;
}


static void wireless_setup_end(void) {
    vTaskDelay(pdMS_TO_TICKS(WIRELESS_SETUP_END_MS));
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
        return;     // the station left meanwhile: the hotspot stays, the unit's only network
    }
    wireless_hotspot_stop();
    printf("Wi-Fi: set up; the hotspot stopped, on %s alone\n", wireless_started_with.ssid);
    update_poll_station_joined();
}


static void wireless_rejoin(void) {
    if (!wireless_started_with.enable || cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        return;
    }
    const char *password = wireless_started_with.auth != AUTH_OPEN ? wireless_started_with.pw : NULL;
    printf("Wi-Fi: not joined; trying %s again\n", wireless_started_with.ssid);
    wireless_station_set(STATION_JOINING, wireless_started_with.ssid, NULL);
    int joined = cyw43_arch_wifi_connect_timeout_ms(wireless_started_with.ssid, password,
                                                    get_cyw43_auth(wireless_started_with.auth),
                                                    WIRELESS_REJOIN_TIMEOUT_MS);
    wireless_last_join = joined;
    if (joined != PICO_OK) {
        wireless_station_set(STATION_FAILED, wireless_started_with.ssid, wireless_join_reason(joined));
        printf("Wi-Fi: not joined (%d)\n", joined);
        return;
    }
    wireless_station_set(STATION_JOINED, wireless_started_with.ssid, NULL);
    wireless_hotspot_stop();
    wireless_station_mdns();
    printf("Wi-Fi: joined %s\n", wireless_started_with.ssid);
    // An image on trial is on its network now; the start's poll, if it has not run (ADR-010, gate 5)
    update_network_ready(true);
    update_poll_station_joined();
}


// The console's `wifi leave` (ADR-004): the station leaves its network, as when its router goes away
bool wireless_station_leave(void) {
    cyw43_arch_lwip_begin();
    int left = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();
    return left == 0;
}


// The server listens (ADR-004): an image on trial confirms itself once the unit is on the network it is set up
// for, a station when Wi-Fi is set up, its own access point when it is not. A unit set up for Wi-Fi that could not
// join, and fell back to its own access point, leaves its trial, and goes back
static void wireless_server_listening(void) {
    bool on_its_network = wireless_started_with.enable
                              ? wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN
                              : wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN;
    update_network_ready(on_its_network);
    // The update's poll, once each start, as a station only: its own access point has no internet (ADR-010, gate 5)
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
        update_poll_station_joined();
    }
}


// Whether a request's Host is the unit's own: its address, or its host name with or without ".local", any port
// (ADR-003: a page that has pointed its own name at the unit is refused)
static bool wireless_host_is_own(const char *host) {
    char name[64];
    size_t len = strcspn(host, ":");
    if (len >= sizeof(name)) {
        return false;
    }
    memcpy(name, host, len);
    name[len] = '\0';
    if (len > 0 && name[len - 1] == '.') {
        name[--len] = '\0';     // a fully qualified name's trailing dot
    }

    char address[24];
    if (wireless_own_address(address, sizeof(address)) && strcmp(name, address) == 0) {
        return true;
    }
    // The station's, while the hotspot is up beside it (the set-up's join, ADR-008, gate 4)
    const ip4_addr_t *station = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
    if (station != NULL && !ip4_addr_isany(station) && ip4addr_ntoa_r(station, address, sizeof(address)) != NULL &&
        strcmp(name, address) == 0) {
        return true;
    }
    size_t host_len = strlen(host_name);
    if (strncasecmp(name, host_name, host_len) == 0 &&
        (name[host_len] == '\0' || strcasecmp(name + host_len, ".local") == 0)) {
        return true;
    }
    return false;
}


static void srv_txt(struct mdns_service *service, void *txt_userdata)
{
  err_t res;
  LWIP_UNUSED_ARG(txt_userdata);

  res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);

  // The API's version and the firmware's, so a client finds which unit runs what before it asks (ADR-003, gate 4)
  res = mdns_resp_add_service_txtitem(service, "api=1", 5);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
  char firmware[64];
  int len = snprintf(firmware, sizeof(firmware), "firmware=%s", version_string);
  if (len > 0 && len < (int) sizeof(firmware)) {
      res = mdns_resp_add_service_txtitem(service, firmware, (u8_t) len);
      LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
  }
}


void wireless_get_link(wireless_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->hostname = host_name;
    switch (wireless_config.current_wireless_state) {
        case WIRELESS_STATE_STA_MODE_INIT:
        case WIRELESS_STATE_STA_MODE_LISTEN:
            info->mode = "station";
            snprintf(info->ssid, sizeof(info->ssid), "%s", wireless_config.eeprom_wireless_metadata.ssid);
            break;
        case WIRELESS_STATE_AP_MODE_INIT:
        case WIRELESS_STATE_AP_MODE_LISTEN:
            info->mode = "access_point";
            snprintf(info->ssid, sizeof(info->ssid), "%s", host_name);     // the access point is named so
            break;
        default:
            info->mode = "none";
            break;
    }
    if (!wireless_own_address(info->address, sizeof(info->address))) {
        info->address[0] = '\0';
    }
    // Why a station set up is not joined (its own access point may stand in meanwhile): its last join's answer
    int last = wireless_last_join;
    if (wireless_config.eeprom_wireless_metadata.enable &&
        wireless_config.current_wireless_state != WIRELESS_STATE_STA_MODE_LISTEN && last != PICO_OK) {
        info->trouble = last == PICO_ERROR_BADAUTH ? "Wrong key" :
                        (last == PICO_ERROR_TIMEOUT ? "Not found" : "Join failed");
    }
}


void wireless_get_info(wireless_info_t *info) {
    wireless_get_link(info);
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
        int32_t rssi = 0;
        cyw43_arch_lwip_begin();
        info->has_rssi = cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0;
        cyw43_arch_lwip_end();
        info->rssi = rssi;
    }
}

void wireless_task(void *p) {
    static TaskHandle_t led_interface_task_handler = NULL;


    wireless_config.current_wireless_state = WIRELESS_STATE_NOT_INITIALIZED;
    wireless_ctrl_queue = xQueueCreate(5, sizeof(wireless_ctrl_t));

    if (cyw43_arch_init()) {
        exit(-1);
    }

    wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;

    // Create LED task
    if (led_interface_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(led_interface_task, "LED Interface Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &led_interface_task_handler);
    }
    else {
        vTaskResume(led_interface_task_handler);
    }

    // What the unit starts with is what it keeps until its restart (the settings' at_restart)
    wireless_started_with = wireless_config.eeprom_wireless_metadata;

    // Start default initialize pattern
    if (wireless_config.eeprom_wireless_metadata.enable) {
        wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_INIT;
        wireless_station_up();

        // If the authentication method is open then the password is NULL
        const char * wifi_password = NULL;
        if (wireless_config.eeprom_wireless_metadata.auth != AUTH_OPEN) {
            wifi_password = wireless_config.eeprom_wireless_metadata.pw;
        }

        // Retry within timeframe. A test build skips it, as if the router were down at the start (ADR-004)
        TickType_t stop_tick = xTaskGetTickCount() + pdMS_TO_TICKS(wireless_config.eeprom_wireless_metadata.timeout_ms);
#if RC_TEST_WIFI_FIRST_JOIN_FAILS || RC_TEST_AP_PROBE
        stop_tick = xTaskGetTickCount();
        printf("Wi-Fi: a test build: the start's join skipped\n");
#endif
        while (xTaskGetTickCount() < stop_tick) {
            int resp;
            resp = cyw43_arch_wifi_connect_timeout_ms(wireless_config.eeprom_wireless_metadata.ssid,
                                                      wifi_password,
                                                      get_cyw43_auth(wireless_config.eeprom_wireless_metadata.auth),
                                                      wireless_config.eeprom_wireless_metadata.timeout_ms);
            wireless_last_join = resp;
            if (resp == PICO_OK) {
                wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
                break;
            }
        }
        if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
            wireless_station_set(STATION_JOINED, wireless_config.eeprom_wireless_metadata.ssid, NULL);
        }
        else if (wireless_last_join != PICO_OK) {
            wireless_station_set(STATION_FAILED, wireless_config.eeprom_wireless_metadata.ssid,
                                 wireless_join_reason(wireless_last_join));
        }
    }

    // If the state didn't change (connection failed) then we shall put it back to idle
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_INIT) {
         wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;
    }


    // If not configured, or failed to connect existing wifi then start the AP mode
    if (wireless_config.current_wireless_state == WIRELESS_STATE_IDLE) {
        // If previous configured, then start STA mode by default
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_INIT;
        wireless_hotspot_on_strongest_channel();
        access_point_mode_start();
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_LISTEN;
    }
    else {
        wireless_station_mdns();
    }

    // The pages and the API: the portal at / on any network, which opens the set-up by itself on a unit never set up
    rest_endpoints_init();

    // Start our own HTTP server (ADR-003, gate 2), in its own task, on lwIP's sockets
    static const http_server_network_t http_network = {
        .host_is_own = wireless_host_is_own,
        .own_address = wireless_own_address,
        .listening = wireless_server_listening,
    };
    http_server_start(&http_network);

    // A station set up and not joined tries again every WIRELESS_REJOIN_MS (ADR-004), by its own clock: as its own
    // access point the unit blinks its LED through this queue, which is then never quiet for that long
    TickType_t rejoin_at = xTaskGetTickCount() + pdMS_TO_TICKS(WIRELESS_REJOIN_MS);
    while (true) {
        wireless_ctrl_t wireless_ctrl;

        TickType_t now = xTaskGetTickCount();
#if RC_TEST_AP_PROBE
        rejoin_at = now + pdMS_TO_TICKS(WIRELESS_REJOIN_MS);   // a probe build joins only when told (`wifi join`)
#endif
        if ((int32_t) (now - rejoin_at) >= 0) {
            wireless_rejoin();
            rejoin_at = xTaskGetTickCount() + pdMS_TO_TICKS(WIRELESS_REJOIN_MS);
            continue;
        }
        if (xQueueReceive(wireless_ctrl_queue, &wireless_ctrl, rejoin_at - now) != pdTRUE) {
            continue;
        }

        switch (wireless_ctrl) {
            case WIRELESS_CTRL_LED_ON:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                break;
            case WIRELESS_CTRL_LED_OFF:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                break;
            case WIRELESS_CTRL_SCAN:
                wireless_scan_run();
                break;
            case WIRELESS_CTRL_JOIN:
                wireless_join_run();
                break;
            case WIRELESS_CTRL_SETUP_END:
                wireless_setup_end();
                break;
            default:
                break;
        }
    }

    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
    cyw43_arch_deinit();

    vTaskSuspend(led_interface_task_handler);
}


