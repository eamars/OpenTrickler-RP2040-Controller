#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_rest.h"
#include "http_server.h"
#include "api_v1.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"
#include "eeprom.h"
#include "mini_12864_module.h"
#include "display.h"
#include "neopixel_led.h"
#include "profile.h"
#include "cleanup_mode.h"
#include "servo_gate.h"
#include "system_control.h"
#include "version.h"

// Generated headers by html2header.py under scripts
#include "display_mirror.html.h"
#include "web_portal.html.h"
#include "wizard.html.h"


// A page in flash: the generated string carries an HTTP header of its own (html2header.py), passed over here so our
// server sends the page with its own (ADR-003, gate 2)
typedef struct {
    const char *body;
    size_t len;
} rest_page_t;

static rest_page_t rest_page_portal;
static rest_page_t rest_page_wizard;
static rest_page_t rest_page_mirror;
static bool rest_root_is_wizard = false;

// A page's tag is the build's: a page changes only with the firmware
static char rest_page_etag[40];


static rest_page_t rest_page_from(const char *generated) {
    rest_page_t page;
    const char *body = strstr(generated, "\r\n\r\n");
    page.body = body != NULL ? body + 4 : generated;
    page.len = strlen(page.body);
    return page;
}


static void rest_send_page(http_server_reply_t *reply, const rest_page_t *page) {
    http_server_reply_static(reply, 200, "text/html; charset=utf-8", page->body, page->len, rest_page_etag);
}


static void rest_get_root(const http_server_request_t *request, http_server_reply_t *reply) {
    rest_send_page(reply, rest_root_is_wizard ? &rest_page_wizard : &rest_page_portal);
}


static void rest_get_portal(const http_server_request_t *request, http_server_reply_t *reply) {
    rest_send_page(reply, &rest_page_portal);
}


static void rest_get_wizard(const http_server_request_t *request, http_server_reply_t *reply) {
    rest_send_page(reply, &rest_page_wizard);
}


static void rest_get_mirror(const http_server_request_t *request, http_server_reply_t *reply) {
    rest_send_page(reply, &rest_page_mirror);
}


bool rest_endpoints_init(bool default_wizard) {
    rest_root_is_wizard = default_wizard;
    rest_page_portal = rest_page_from(html_web_portal_html);
    rest_page_wizard = rest_page_from(html_wizard_html);
    rest_page_mirror = rest_page_from(html_display_mirror_html);
    snprintf(rest_page_etag, sizeof(rest_page_etag), "%s-%s", version_string, vcs_hash);

    // The pages
    http_server_add_route(HTTP_SERVER_GET, "/", rest_get_root);
    http_server_add_route(HTTP_SERVER_GET, "/mobile", rest_get_portal);
    http_server_add_route(HTTP_SERVER_GET, "/wizard", rest_get_wizard);
    http_server_add_route(HTTP_SERVER_GET, "/display_mirror", rest_get_mirror);

    // The old paths, through the adapter, until gate 4 (ADR-003)
    rest_register_handler("/rest/scale_action", http_rest_scale_action);
    rest_register_handler("/rest/scale_config", http_rest_scale_config);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/charge_mode_state", http_rest_charge_mode_state);
    rest_register_handler("/rest/cleanup_mode_state", http_rest_cleanup_mode_state);
    rest_register_handler("/rest/system_control", http_rest_system_control);
    rest_register_handler("/rest/coarse_motor_config", http_rest_coarse_motor_config);
    rest_register_handler("/rest/fine_motor_config", http_rest_fine_motor_config);
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/mini_12864_config", http_rest_mini_12864_module_config);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
    rest_register_handler("/rest/neopixel_led_config", http_rest_neopixel_led_config);
    rest_register_handler("/rest/profile_config", http_rest_profile_config);
    rest_register_handler("/rest/profile_summary", http_rest_profile_summary);
    rest_register_handler("/rest/servo_gate_state", http_rest_servo_gate_state);
    rest_register_handler("/rest/servo_gate_config", http_rest_servo_gate_config);
    rest_register_handler("/display_buffer", http_get_display_buffer);

    // API v1
    return api_v1_init();
}
