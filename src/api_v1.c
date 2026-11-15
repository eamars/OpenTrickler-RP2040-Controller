/*
 * API v1's routes, gate 2's share (ADR-003): GET /api/v1/charge, /api/v1/display and /api/v1/display/frame, and
 * under /api/v1/debug, which is not part of v1, the figures and two routes for gate 2's proofs: a reply the size of
 * a throw's samples (/debug/load) and a parked request (/debug/wait).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <u8g2.h>

#include "lwip/stats.h"
#include "lwip/memp.h"

#include "api_v1.h"
#include "http_server.h"
#include "http_json.h"
#include "charge_mode.h"
#include "profile.h"
#include "scale.h"
#include "display.h"
#include "settings.h"


extern charge_mode_config_t charge_mode_config;

// /debug/load's bounds: well past a throw's 650 rows of 16 columns
#define API_V1_LOAD_ROWS_MAX 2000
#define API_V1_LOAD_COLUMNS_MAX 32
#define API_V1_WAIT_S_MAX 30


static uint32_t api_v1_now_ms(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


static const char *api_v1_charge_state(charge_mode_state_t state) {
    switch (state) {
        case CHARGE_MODE_EXIT: return "idle";
        case CHARGE_MODE_WAIT_FOR_ZERO: return "zeroing";
        case CHARGE_MODE_WAIT_FOR_COMPLETE: return "charging";
        case CHARGE_MODE_WAIT_FOR_CUP_REMOVAL: return "waiting_for_cup_removal";
        case CHARGE_MODE_WAIT_FOR_CUP_RETURN: return "waiting_for_cup_return";
        default: return "other";
    }
}


// GET /api/v1/charge: what gate 2 has of it; gate 4 adds the rest (api_v1.md, "Charge")
static void api_v1_get_charge(const http_server_request_t *request, http_server_reply_t *reply) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;

    // The profile's name, ended within its 16 bytes: it may be stored with none (audit)
    char name[sizeof(profile_get_selected()->name) + 1];
    memcpy(name, profile_get_selected()->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    float target = charge_mode_config.target_charge_weight;
    http_json_raw(out, room, &len, "{\"state\":");
    http_json_string(out, room, &len, api_v1_charge_state(charge_mode_config.charge_mode_state));
    http_json_raw(out, room, &len, ",\"target\":");
    if (target > 0.0f) {
        http_json_number(out, room, &len, target, 9);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, ",\"powder_profile\":");
    http_json_integer(out, room, &len, profile_get_selected_idx());
    http_json_raw(out, room, &len, ",\"powder_profile_name\":");
    http_json_string(out, room, &len, name);
    http_json_raw(out, room, &len, ",\"weight\":");
    http_json_number(out, room, &len, scale_get_current_measurement(), 6);
    http_json_raw(out, room, &len, ",\"elapsed_s\":");
    http_json_number(out, room, &len, charge_mode_get_elapsed_s(), 4);
    http_json_raw(out, room, &len, "}");

    if (!http_json_fits(room, len)) {
        http_server_reply_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    http_server_reply_send(reply, 200, "application/json", len);
}


// GET /api/v1/display
static void api_v1_get_display(const http_server_request_t *request, http_server_reply_t *reply) {
    u8g2_t *display = get_display_handler();
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    int len = snprintf(out, room, "{\"width\":%u,\"height\":%u,\"format\":\"u8g2_tiles\"}",
                       (unsigned) u8g2_GetBufferTileWidth(display) * 8,
                       (unsigned) u8g2_GetBufferTileHeight(display) * 8);
    http_server_reply_send(reply, 200, "application/json", (size_t) len);
}


// GET /api/v1/display/frame: the screen's bytes, copied at once, so the reply is one frame
static void api_v1_get_display_frame(const http_server_request_t *request, http_server_reply_t *reply) {
    u8g2_t *display = get_display_handler();
    size_t size = 8 * (size_t) u8g2_GetBufferTileHeight(display) * u8g2_GetBufferTileWidth(display);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    if (size > room) {
        http_server_reply_error(reply, 500, "failed", "the frame is larger than a reply");
        return;
    }
    memcpy(out, u8g2_GetBufferPtr(display), size);
    http_server_reply_send(reply, 200, "application/octet-stream", size);
}


static void api_v1_json_pool(char *out, size_t room, size_t *len, const char *name, memp_t pool) {
    const struct stats_mem *mem = lwip_stats.memp[pool];
    char text[112];
    snprintf(text, sizeof(text), "\"%s\":{\"used\":%u,\"max\":%u,\"size\":%u,\"err\":%lu}", name,
             (unsigned) mem->used, (unsigned) mem->max, (unsigned) mem->avail, (unsigned long) mem->err);
    http_json_raw(out, room, len, text);
}


// GET /api/v1/debug: figures for the soak and the proofs. Not part of v1
static void api_v1_get_debug(const http_server_request_t *request, http_server_reply_t *reply) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_server_stats_t server;
    http_server_get_stats(&server);
    char text[384];     // the server's line, every count at 10 digits: about 305

    snprintf(text, sizeof(text), "{\"heap_free\":%u,\"heap_min\":%u,\"net\":{\"heap\":{\"used\":%u,\"max\":%u,"
             "\"size\":%u,\"err\":%lu},", (unsigned) xPortGetFreeHeapSize(),
             (unsigned) xPortGetMinimumEverFreeHeapSize(), (unsigned) lwip_stats.mem.used,
             (unsigned) lwip_stats.mem.max, (unsigned) lwip_stats.mem.avail, (unsigned long) lwip_stats.mem.err);
    http_json_raw(out, room, &len, text);
    api_v1_json_pool(out, room, &len, "tcp_pcb", MEMP_TCP_PCB);
    http_json_raw(out, room, &len, ",");
    api_v1_json_pool(out, room, &len, "tcp_seg", MEMP_TCP_SEG);
    http_json_raw(out, room, &len, ",");
    api_v1_json_pool(out, room, &len, "pbuf", MEMP_PBUF);
    http_json_raw(out, room, &len, ",");
    api_v1_json_pool(out, room, &len, "pbuf_pool", MEMP_PBUF_POOL);
    http_json_raw(out, room, &len, ",");
    api_v1_json_pool(out, room, &len, "netconn", MEMP_NETCONN);
    snprintf(text, sizeof(text), "},\"server\":{\"accepted\":%lu,\"requests\":%lu,\"closed_idle\":%lu,"
             "\"closed_for_room\":%lu,\"aborted_stalled\":%lu,\"refused\":%lu,\"send_no_memory\":%lu,"
             "\"send_failed\":%lu,\"stream_make_ms\":%lu,\"stream_bytes\":%lu,\"slots_used\":%u,"
             "\"slots_parked\":%u}}",
             (unsigned long) server.accepted, (unsigned long) server.requests, (unsigned long) server.closed_idle,
             (unsigned long) server.closed_for_room, (unsigned long) server.aborted_stalled,
             (unsigned long) server.refused, (unsigned long) server.send_no_memory,
             (unsigned long) server.send_failed, (unsigned long) server.stream_make_ms,
             (unsigned long) server.stream_bytes, server.slots_used, server.slots_parked);
    http_json_raw(out, room, &len, text);

    if (!http_json_fits(room, len)) {
        http_server_reply_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    http_server_reply_send(reply, 200, "application/json", len);
}


// /debug/load: rows of numbers written as a recording's samples will be (six significant digits), so the cost of
// a tuning session's data to the charge can be measured now (ADR-003, gate 2)
typedef struct {
    uint16_t rows;
    uint16_t columns;
    uint16_t row;
    uint8_t phase;          // 0 the columns, 1 the rows, 2 the end
} api_v1_load_t;


static bool api_v1_load_more(void *ctx, char *buf, size_t room, size_t *len) {
    api_v1_load_t *load = ctx;
    *len = 0;
    if (load->phase == 0) {
        http_json_raw(buf, room, len, "{\"columns\":[");
        for (uint16_t col = 0; col < load->columns; col += 1) {
            char name[12];
            snprintf(name, sizeof(name), "%s\"c%u\"", col ? "," : "", (unsigned) col);
            http_json_raw(buf, room, len, name);
        }
        http_json_raw(buf, room, len, "],\"rows\":[");
        load->phase = 1;
        return true;
    }
    if (load->phase == 1) {
        // A row of 16 columns is about 120 bytes: rows while one more surely fits
        while (load->row < load->rows && *len + 16 + 14 * (size_t) load->columns < room) {
            http_json_raw(buf, room, len, load->row ? ",[" : "[");
            for (uint16_t col = 0; col < load->columns; col += 1) {
                if (col) {
                    http_json_raw(buf, room, len, ",");
                }
                float value = 30.0f * sinf(0.01f * (float) load->row + 0.7f * (float) col) + (float) col;
                http_json_number(buf, room, len, value, 6);
            }
            http_json_raw(buf, room, len, "]");
            load->row += 1;
        }
        if (load->row >= load->rows) {
            load->phase = 2;
        }
        return true;
    }
    http_json_raw(buf, room, len, "]}");
    return false;
}


static long api_v1_query_long(const char *query, const char *name, long fallback) {
    char value[16];
    if (!http_server_query_value(query, name, value, sizeof(value)) || value[0] == '\0') {
        return fallback;
    }
    return strtol(value, NULL, 10);
}


// GET /api/v1/debug/load?rows=600&columns=16
static void api_v1_get_debug_load(const http_server_request_t *request, http_server_reply_t *reply) {
    long rows = api_v1_query_long(request->query, "rows", 600);
    long columns = api_v1_query_long(request->query, "columns", 16);
    if (rows < 1 || rows > API_V1_LOAD_ROWS_MAX || columns < 1 || columns > API_V1_LOAD_COLUMNS_MAX) {
        http_server_reply_error(reply, 400, "out_of_range", "rows 1 to 2000, columns 1 to 32");
        return;
    }
    api_v1_load_t *load = http_server_reply_context(reply, sizeof(api_v1_load_t));
    load->rows = (uint16_t) rows;
    load->columns = (uint16_t) columns;
    load->row = 0;
    load->phase = 0;
    http_server_reply_stream(reply, 200, "application/json", api_v1_load_more, load);
}


// /debug/wait: a parked request, woken by a condition (the unit's clock past a time) or by its own time
typedef struct {
    uint32_t until_ms;
} api_v1_wait_t;


static bool api_v1_wait_ready(void *ctx) {
    const api_v1_wait_t *wait = ctx;
    return (int32_t) (api_v1_now_ms() - wait->until_ms) >= 0;
}


static void api_v1_wait_resume(const http_server_request_t *request, http_server_reply_t *reply) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    int len = snprintf(out, room, "{\"woke\":\"%s\",\"uptime_ms\":%lu}",
                       http_server_reply_woke_ready(reply) ? "condition" : "time", (unsigned long) api_v1_now_ms());
    http_server_reply_send(reply, 200, "application/json", (size_t) len);
}


// GET /api/v1/debug/wait?until_ms=N&wait=S: held until the unit's clock reaches N ms, or for S seconds at most
static void api_v1_get_debug_wait(const http_server_request_t *request, http_server_reply_t *reply) {
    long until_ms = api_v1_query_long(request->query, "until_ms", 0);
    long wait_s = api_v1_query_long(request->query, "wait", 5);
    if (wait_s < 1 || wait_s > API_V1_WAIT_S_MAX || until_ms < 0) {
        http_server_reply_error(reply, 400, "out_of_range", "wait 1 to 30 s, until_ms 0 or more");
        return;
    }
    api_v1_wait_t *wait = http_server_reply_context(reply, sizeof(api_v1_wait_t));
    wait->until_ms = (uint32_t) until_ms;
    http_server_reply_park(reply, api_v1_wait_ready, wait, (uint32_t) wait_s * 1000, api_v1_wait_resume);
}


// GET /api/v1/powder_profiles: saved, and each profile's id and name (gate 3). Which the next throw uses is the
// charge's powder_profile
static void api_v1_get_powder_profiles(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"saved\":");
    http_json_bool(out, room, &len, profile_data_is_saved());
    http_json_raw(out, room, &len, ",\"profiles\":[");
    for (uint16_t idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        http_json_raw(out, room, &len, idx ? ",{\"id\":" : "{\"id\":");
        http_json_integer(out, room, &len, idx);
        http_json_raw(out, room, &len, ",\"name\":");
        http_json_string(out, room, &len, profile_get_name(idx));
        http_json_raw(out, room, &len, "}");
    }
    http_json_raw(out, room, &len, "]}");
    if (!http_json_fits(room, len)) {
        http_server_reply_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    http_server_reply_send(reply, 200, "application/json", len);
}


bool api_v1_init(void) {
    bool ok = true;
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/powder_profiles", api_v1_get_powder_profiles);
    // Every module's settings' table, registered at its start (gate 3)
    settings_add_routes();
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/charge", api_v1_get_charge);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/display", api_v1_get_display);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/display/frame", api_v1_get_display_frame);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug", api_v1_get_debug);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug/load", api_v1_get_debug_load);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug/wait", api_v1_get_debug_wait);
    return ok;
}
