#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <pico/cyw43_arch.h>
#include "dhcpserver.h"
#include "dnsserver.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/apps/httpd.h"

#include "app.h"
#include "mini_12864_module.h"
#include "u8g2.h"
#include "pico/unique_id.h"
#include "display.h"
#include "eeprom.h"



static dhcp_server_t dhcp_server;
static dns_server_t dns_server;
static ip4_addr_t mask;
static ip_addr_t gw;

extern char ip_addr_string[16];
extern char first_line_buffer[32];
extern char second_line_buffer[32];
extern char host_name[18];

bool access_point_mode_start() {
    char ap_password[] = "opentrickler";

    cyw43_arch_enable_ap_mode(host_name, ap_password, CYW43_AUTH_WPA2_AES_PSK);

    // Initialize IP
    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);  
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);

    // Start the dns server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &gw);

    sprintf(first_line_buffer, ">%s", host_name);
    sprintf(second_line_buffer, ">%s", ap_password);

    return true;
}


bool access_point_mode_stop() {
    dhcp_server_deinit(&dhcp_server);
    dns_server_deinit(&dns_server);

    memset(first_line_buffer, 0x0, sizeof(first_line_buffer));
    memset(second_line_buffer, 0x0, sizeof(second_line_buffer));

    return true;
}