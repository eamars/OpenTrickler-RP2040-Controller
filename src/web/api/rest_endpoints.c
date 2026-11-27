#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_server.h"
#include "api_v1.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"
#include "eeprom.h"
#include "neopixel_led.h"
#include "profile.h"
#include "cleanup_mode.h"
#include "servo_gate.h"
#include "system_control.h"
#include "version.h"

// The portal, gzipped by scripts/web_build.py at build time (ADR-008, gate 3)
#include "web_portal.gz.h"


// A page's tag is the build's: a page changes only with the firmware
static char rest_page_etag[40];


// features: 1 3 6
// The portal (ADR-008, gate 3): one page, gzipped, at /, /mobile and /display_mirror (it opens its Unit page there,
// where the mirror is), on any network: on its own hotspot, a unit never set up opens the set-up by itself (gate 4).
// Every browser takes gzip; a client that asks for none is sent it all the same
static void rest_get_portal(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    http_server_reply_static_gzip(reply, 200, "text/html; charset=utf-8", (const char *) web_portal_gz,
                                  web_portal_gz_len, rest_page_etag);
}


// features: 4
// The wizard's old address: the portal's set-up, on any network (ADR-008, gate 4: the wizard is gone)
static void rest_get_wizard(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    http_server_reply_redirect(reply, "/#setup");
}


// Before a restart: each client told the unit has gone, not left with a request that hangs
static void rest_endpoints_before_restart(void) {
    http_server_close_all(1000);
}


bool rest_endpoints_init(void) {
    system_before_restart(rest_endpoints_before_restart);
    snprintf(rest_page_etag, sizeof(rest_page_etag), "%s-%s", version_string, vcs_hash);

    // The pages
    http_server_add_route(HTTP_SERVER_GET, "/", rest_get_portal);
    http_server_add_route(HTTP_SERVER_GET, "/mobile", rest_get_portal);
    http_server_add_route(HTTP_SERVER_GET, "/wizard", rest_get_wizard);
    http_server_add_route(HTTP_SERVER_GET, "/display_mirror", rest_get_portal);

    // API v1: the only API (ADR-003, gate 4: the old /rest/ paths, their adapter and handlers are gone)
    return api_v1_init();
}
