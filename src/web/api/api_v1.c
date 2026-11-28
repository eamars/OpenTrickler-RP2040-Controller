/*
 * API v1's routes (ADR-003): the index, the powder profiles, the motors' settings listed, the display and its frame;
 * the settings' come from their tables, the commands' from api_v1_commands.c. Under /api/v1/debug, which is not part
 * of v1, the figures and the routes for the proofs: a reply the size of a throw's samples (/debug/load), a parked
 * request (/debug/wait), and a body taken in pieces (/debug/upload).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "lwip/stats.h"
#include "lwip/memp.h"

#include "api_v1.h"
#include "system_control.h"
#include "http_server.h"
#include "http_json.h"
#include "charge_mode.h"
#include "charge_profiles.h"
#include "common.h"
#include "profile.h"
#include "scale.h"
#include "eeprom.h"
#include "unit_id.h"
#include "gate.h"
#include "ot_features.h"
#include "settings.h"
#include "version.h"
#include "wireless.h"

#include "chip.h"

#include "agent_guide.h"    // made from agent_guide.md as the build configures (ADR-016, gate 2)



// /debug/load's bounds: well past a throw's 650 rows of 16 columns
#define API_V1_LOAD_ROWS_MAX 2000
#define API_V1_LOAD_COLUMNS_MAX 32
#define API_V1_WAIT_S_MAX 30

// /debug/upload's: those whole, and those cut off (the client gone, or stalled)
static uint32_t api_v1_uploads_done = 0;
static uint32_t api_v1_uploads_aborted = 0;


static uint32_t api_v1_now_ms(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


// The screen the UI handed over (api_v1.h); NULL until then
static const api_v1_screen_t *api_v1_screen_handed = NULL;


void api_v1_set_screen(const api_v1_screen_t *screen) {
    api_v1_screen_handed = screen;
}


const api_v1_screen_t *api_v1_screen(void) {
    return api_v1_screen_handed;
}


// GET /api/v1/display
// GET /api/v1/agent and /llms.txt (ADR-016, gate 2): the guide for an AI agent a person points at the unit, as it is,
// not compressed, so any client reads it
static void api_v1_get_agent_guide(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    http_server_reply_static(reply, 200, "text/markdown; charset=utf-8", agent_guide_text,
                             sizeof(agent_guide_text) - 1, NULL);
}


static void api_v1_get_display(const http_server_request_t *request, http_server_reply_t *reply) {
    const api_v1_screen_t *screen = api_v1_screen();
    if (screen == NULL) {
        http_server_reply_error(reply, 404, "not_found", "this build has no screen");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    int len = snprintf(out, room, "{\"width\":%u,\"height\":%u,\"format\":\"%s\"}", (unsigned) screen->width,
                       (unsigned) screen->height, screen->format);
    http_server_reply_send(reply, 200, "application/json", (size_t) len);
}


// GET /api/v1/display/frame: the screen's frame, copied whole, so the reply is one frame (the display's `format`)
static void api_v1_get_display_frame(const http_server_request_t *request, http_server_reply_t *reply) {
    const api_v1_screen_t *screen = api_v1_screen();
    if (screen == NULL) {
        http_server_reply_error(reply, 404, "not_found", "this build has no screen");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    if (screen->frame_bytes > room) {
        http_server_reply_error(reply, 500, "failed", "the frame is larger than a reply");
        return;
    }
    screen->frame_copy((uint8_t *) out);
    http_server_reply_send(reply, 200, "application/octet-stream", screen->frame_bytes);
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
    char text[448];     // the server's line, every count at 10 digits: about 370

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
             "\"send_failed\":%lu,\"stream_make_ms\":%lu,\"stream_bytes\":%lu,\"stream_failed\":%lu,\"slots_used\":%u,"
             "\"slots_parked\":%u,\"slots_live\":%u,\"live_opened\":%lu,\"live_refused\":%lu},",
             (unsigned long) server.accepted, (unsigned long) server.requests, (unsigned long) server.closed_idle,
             (unsigned long) server.closed_for_room, (unsigned long) server.aborted_stalled,
             (unsigned long) server.refused, (unsigned long) server.send_no_memory,
             (unsigned long) server.send_failed, (unsigned long) server.stream_make_ms,
             (unsigned long) server.stream_bytes, (unsigned long) server.stream_failed, server.slots_used,
             server.slots_parked, server.slots_live, (unsigned long) server.live_opened,
             (unsigned long) server.live_refused);
    http_json_raw(out, room, &len, text);
    // The settings' EEPROM's size, told without writing (ADR-014): 32 or 16 KB; 0, not told
    snprintf(text, sizeof(text), "\"uploads\":{\"done\":%lu,\"aborted\":%lu},\"eeprom_kb\":%u}",
             (unsigned long) api_v1_uploads_done, (unsigned long) api_v1_uploads_aborted,
             (unsigned) config_eeprom_kb());
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


/*
    PUT /api/v1/debug/upload: bytes taken in pieces as they come, counted and summed by CRC-32, and dropped. Gate 4
    shows with it what ADR-004's image will go through: a body of 700 KB during a charge. The counts of those whole and
    those cut off are in /api/v1/debug
*/
#define API_V1_UPLOAD_MAX (4u * 1024u * 1024u)

typedef struct {
    uint32_t bytes;
    uint32_t crc;
    uint32_t pieces;
    uint32_t largest;
    uint32_t started_ms;
} api_v1_upload_t;

static bool api_v1_upload_begin(const http_server_request_t *request, http_server_reply_t *reply) {
    if (request->body_is_json) {
        http_server_reply_error(reply, 415, "wrong_media_type", "the bytes are sent as application/octet-stream");
        return false;
    }
    if (request->body_len > API_V1_UPLOAD_MAX) {
        http_server_reply_error(reply, 413, "too_large", "4 MB at most");
        return false;
    }
    api_v1_upload_t *upload = http_server_reply_context(reply, sizeof(api_v1_upload_t));
    memset(upload, 0, sizeof(*upload));
    upload->started_ms = api_v1_now_ms();
    return true;
}


static bool api_v1_upload_piece(void *ctx, const char *data, size_t len) {
    api_v1_upload_t *upload = ctx;
    upload->crc = software_crc32_continue(upload->crc, data, len);
    upload->bytes += (uint32_t) len;
    upload->pieces += 1;
    if (len > upload->largest) {
        upload->largest = (uint32_t) len;
    }
    return true;
}


static void api_v1_upload_end(const http_server_request_t *request, http_server_reply_t *reply) {
    api_v1_upload_t *upload = http_server_reply_context(reply, sizeof(api_v1_upload_t));
    api_v1_uploads_done += 1;
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    int len = snprintf(out, room, "{\"bytes\":%lu,\"crc32\":%lu,\"pieces\":%lu,\"largest\":%lu,\"ms\":%lu}",
                       (unsigned long) upload->bytes, (unsigned long) upload->crc, (unsigned long) upload->pieces,
                       (unsigned long) upload->largest, (unsigned long) (api_v1_now_ms() - upload->started_ms));
    http_server_reply_send(reply, 200, "application/json", (size_t) len);
}


static void api_v1_upload_abort(void *ctx) {
    api_v1_uploads_aborted += 1;
}


static const http_server_upload_t api_v1_debug_upload = {
    .begin = api_v1_upload_begin,
    .piece = api_v1_upload_piece,
    .end = api_v1_upload_end,
    .abort = api_v1_upload_abort,
};


/*
    GET /api/v1, the index (gate 3): what the unit is, a guide in plain words, and each path with its methods. The
    settings' paths come from their tables; the rest are below. /api/v1/debug is not part of v1, and not listed.
*/
typedef struct {
    const char *path;
    const char *methods;    // as JSON: a list
    const char *about;
    bool (*present)(void);  // NULL: always listed; else only while it answers true (a part a build may leave out)
} api_v1_resource_t;

static const api_v1_resource_t api_v1_resources[] = {
    {"/api/v1", "[\"GET\"]", "this index"},
    {"/api/v1/agent", "[\"GET\"]",
     "for an AI agent helping the person at the unit: what to do first and how to help, in Markdown. Read it first"},
    {"/api/v1/schema", "[\"GET\"]",
     "every settings resource's fields: type, unit, limits or choices, access, when each applies and when it is "
     "refused; every action with its body's fields; every state and event kind; the error codes"},
    {"/api/v1/settings", "[\"GET\"]",
     "every setting, powder profile and charge profile in one document, with the firmware's version: the export. An "
     "import is a PATCH of each resource in turn"},
    {"/api/v1/settings/save", "[\"POST\"]", "stores every setting, the powder profiles and the charge profiles"},
    {"/api/v1/powder_profiles", "[\"GET\"]", "the powder profiles' ids and names, and whether they are stored"},
    {"/api/v1/charge_profiles", "[\"GET\"]",
     "the charge profiles' ids and names, and whether they and the one chosen are stored"},
    {"/api/v1/settings/motors", "[\"GET\"]",
     "the motors whose settings the unit holds, each by its id and label, and whether they are stored"},
    {"/api/v1/charge","[\"GET\",\"PATCH\"]",
     "the charge: its state, the throw, the charge profile chosen with its target and powder profile, the weight, "
     "the last throw; a PATCH chooses the charge profile, or sets the chosen one's target or powder, for the next "
     "throw"},
    {"/api/v1/charge/start", "[\"POST\"]",
     "starts a charge: {}, or the charge profile to choose, the target and the powder profile to set first"},
    {"/api/v1/charge/stop", "[\"POST\"]", "stops the charge, and acknowledges its fault; never fails"},
    {"/api/v1/charge/throws", "[\"GET\"]",
     "each throw's record after a number (?after=N, ?wait=s as the events), the newest 32: what it was thrown on "
     "and what it did (the schema's throw)"},
    {"/api/v1/purge", "[\"GET\",\"PATCH\"]",
     "the purge: its state and speed; a PATCH sets the speed, with a time to run no longer than"},
    {"/api/v1/purge/start", "[\"POST\"]", "starts a purge, at speed 0"},
    {"/api/v1/purge/stop", "[\"POST\"]", "stops the purge, and acknowledges its fault; never fails"},
    {"/api/v1/stop", "[\"POST\"]", "stops whatever job runs; never fails"},
    {"/api/v1/calibration", "[\"GET\"]",
     "the calibration (ADR-009 step 1): its part and step, what it does, the records it found, and what is kept"},
    {"/api/v1/calibration/start", "[\"POST\"]", "starts a part of the calibration: cup, scale or powder"},
    {"/api/v1/calibration/answer", "[\"POST\"]", "the person's answer at a step of the calibration"},
    {"/api/v1/calibration/pour", "[\"POST\"]", "while the cup is filled: the coarse tube turned at least these turns from now"},
    {"/api/v1/calibration/save", "[\"POST\"]", "keeps and stores the records the calibration found"},
    {"/api/v1/calibration/stop", "[\"POST\"]", "stops the calibration, discarding what it found; never fails"},
    {"/api/v1/agent/hello", "[\"POST\"]",
     "for agents: say hello, and the screen shows who is watching for 2 s; never over a charge"},
    {"/api/v1/events", "[\"GET\"]",
     "the unit's changes after a number (?after=N), held until there is one with ?wait=s"},
    {"/api/v1/events/stream", "[\"GET\"]",
     "the same changes as they happen, as server-sent events: first the unit (its boot, and whether it goes on from "
     "the client's Last-Event-ID or ?after=N, nothing missed), then the state (the job, the charge with its weight, "
     "the purge), then each event with its id, and the state again as it changes and after 15 s quiet. At most "
     "3 at once: 503 then, with Retry-After"},
    {"/api/v1/scale", "[\"GET\"]", "the scale: its weight and unit, the driver that runs, its rhythm"},
    {"/api/v1/scale/zero", "[\"POST\"]", "asks the scale to zero, where it may: see the guide"},
    {"/api/v1/gate", "[\"GET\",\"PATCH\"]", "the servo gate: its state and ratio; a PATCH moves it", gate_present},
    {"/api/v1/motors", "[\"GET\"]", "each motor's speed and its driver's state"},
    {"/api/v1/system", "[\"GET\"]",
     "the firmware, the board, the uptime, what the unit is doing, a restart pending, whether it is set up"},
    {"/api/v1/system/restart", "[\"POST\"]", "restarts the unit, once the reply is out"},
    {"/api/v1/system/factory_reset", "[\"POST\"]",
     "erases the stored settings and restarts: the unit comes back as its own access point"},
    {"/api/v1/network", "[\"GET\"]",
     "the network the unit is on: its mode, name, address, host name, signal; the station's last join and its end"},
    {"/api/v1/wifi/networks", "[\"GET\"]", "the networks the last scan heard, strongest first, and whether one runs"},
    {"/api/v1/wifi/scan", "[\"POST\"]", "scans for networks: twice, merged by name"},
    {"/api/v1/wifi/join", "[\"POST\"]",
     "the set-up's join, while the unit is its own hotspot, kept up meanwhile: stored once joined, never if refused"},
    {"/api/v1/setup", "[\"GET\",\"PATCH\"]",
     "the set-up's stage, kept by the unit: set up or not, the step, the network chosen, a phone on its hotspot, the "
     "hotspot's name; a PATCH gives the page's step"},
    {"/api/v1/setup/done", "[\"POST\"]",
     "the set-up finished or skipped: remembered; joined, the hotspot stops a moment after the reply"},
    {"/api/v1/update", "[\"GET\"]",
     "the firmware update (ADR-004): its state, the image running and its slot, the image this unit takes, the last"},
    {"/api/v1/update/start", "[\"POST\"]",
     "starts an update: the menu takes it, then the image is PUT, or the job downloads the release found"},
    {"/api/v1/update/check", "[\"POST\"]", "polls the update's source now for a newer release"},
    {"/api/v1/update/image", "[\"PUT\"]", "the update image, as application/octet-stream, into the free slot"},
    {"/api/v1/update/install", "[\"POST\"]", "restarts into a trial of the image received"},
    {"/api/v1/update/stop", "[\"POST\"]", "stops the update; never fails"},
    {"/api/v1/display", "[\"GET\"]", "the screen's size and the format of its frame"},
    {"/api/v1/display/frame", "[\"GET\"]", "the screen's frame, in the format /api/v1/display names"},
};

static const char api_v1_guide[] =
    "A GET reads and changes nothing. A setting is written with a PATCH of its resource, any of its fields at "
    "once, checked whole: if one field is refused, nothing is written. A setting written is live at once, or "
    "when its schema's 'applies' says; 'saved' in each settings reply says whether what runs is what is stored, "
    "and a POST of {} to the resource's save path stores it. A write may be refused for a moment with 409 busy: "
    "the charge's, the scale's and the gate's settings and the powder profile in use while a throw is under way, "
    "the motors' while the charge or a purge runs. GET /api/v1/schema describes every field. Weights are in the "
    "scale's own unit. "
    "The unit runs one job at a time: a charge or a purge. A charge throws the chosen charge profile's target "
    "(GET /api/v1/charge_profiles). It is started with POST /api/v1/charge/start "
    "once a target is set, and throws until it is stopped: each throw waits for the scale to read zero, pours to "
    "the target, gives its verdict, and waits for the cup to be lifted and put back. POST /api/v1/stop stops "
    "whatever job runs, and never fails; a job that ended on a fault waits in 'fault' until a stop or a press at "
    "the unit. Follow GET /api/v1/events?after=N&wait=30 to see each change, from where you left off. "
    "The unit cannot know what is in the cup, nor whether the hopper holds powder: a zero and then a start pours "
    "onto whatever is on the pan. Start, and zero, only when a person has said an empty cup is on the pan.";

// The guide is one item of the index's stream, sent whole in a chunk: the server's buffer less its chunk's head and
// end (10 bytes), with the guide's quotes and the 14 bytes that follow it. Longer, the index is cut off at the guide
_Static_assert(sizeof(api_v1_guide) + 2 + 14 < HTTP_SERVER_BUFFER_SIZE - 10, "the guide no longer fits a chunk");

static uint32_t api_v1_boot = 0;        // new at each start, so a client sees a restart (api_v1.md, the index)


uint32_t api_v1_boot_id(void) {
    return api_v1_boot;
}

typedef struct {
    uint8_t phase;      // 0 the head, 1 the guide, 2 api_v1_resources, 3 the settings' paths, 4 their saves, 5 the end
    uint8_t idx;
} api_v1_index_t;


static void api_v1_index_resource(char *out, size_t room, size_t *len, bool first, const char *path,
                                  const char *methods, const char *about) {
    http_json_raw(out, room, len, first ? "{\"path\":" : ",{\"path\":");
    http_json_string(out, room, len, path);
    http_json_raw(out, room, len, ",\"methods\":");
    http_json_raw(out, room, len, methods);
    http_json_raw(out, room, len, ",\"about\":");
    http_json_string(out, room, len, about);
    http_json_raw(out, room, len, "}");
}


// One item of the index: its head, the guide, a path, a settings' path, a save with its revert. False after the last
static bool api_v1_index_item(api_v1_index_t *index, char *buf, size_t room, size_t *len) {
    if (index->phase == 0) {
        char unit_id[UNIT_ID_LENGTH + 1];
        unit_id_get(unit_id, sizeof(unit_id));
        http_json_raw(buf, room, len, "{\"api\":\"1\",\"firmware\":{\"version\":");
        http_json_string(buf, room, len, version_string);
        http_json_raw(buf, room, len, ",\"vcs\":");
        http_json_string(buf, room, len, vcs_hash);
        http_json_raw(buf, room, len, ",\"build\":");
        http_json_string(buf, room, len, build_type);
        http_json_raw(buf, room, len, "},\"board\":");
        http_json_string(buf, room, len, system_board_name());
        http_json_raw(buf, room, len, ",\"unit_id\":");
        http_json_string(buf, room, len, unit_id);
        http_json_raw(buf, room, len, ",\"hostname\":");
        http_json_string(buf, room, len, wireless_host_name());
        http_json_raw(buf, room, len, ",\"boot\":");
        http_json_unsigned(buf, room, len, api_v1_boot);
        http_json_raw(buf, room, len, ",\"uptime_s\":");
        http_json_integer(buf, room, len, (long) (chip_time_us() / 1000000u));
        // What this build carries (ADR-008, gate 2): each part a build may leave out, by name, present or not, made
        // from the board's list (ADR-006, gate 9)
        http_json_raw(buf, room, len, ",\"features\":[");
        for (const ot_feature_t *feature = ot_features; feature->name != NULL; feature += 1) {
            http_json_raw(buf, room, len, feature == ot_features ? "{\"name\":" : ",{\"name\":");
            http_json_string(buf, room, len, feature->name);
            http_json_raw(buf, room, len, ",\"present\":");
            http_json_bool(buf, room, len, feature->present);
            http_json_raw(buf, room, len, ",\"about\":");
            http_json_string(buf, room, len, feature->about);
            http_json_raw(buf, room, len, "}");
        }
        // The guide for an AI agent, by its path: the index's own guide has no room left (ADR-016, gate 2)
        http_json_raw(buf, room, len, "],\"agent\":\"/api/v1/agent\",\"pin\":");
        // Whether writes need the PIN on the unit's Wi-Fi page (ADR-016, gate 3); never the PIN
        http_json_bool(buf, room, len, wireless_pin_required());
        http_json_raw(buf, room, len, ",\"guide\":");
        index->phase = 1;
        return true;
    }
    if (index->phase == 1) {
        http_json_string(buf, room, len, api_v1_guide);
        http_json_raw(buf, room, len, ",\"resources\":[");
        index->phase = 2;
        return true;
    }
    if (index->phase == 2) {
        // A path a chunk, past those of a part this unit does not have (the first is always listed)
        const size_t count = sizeof(api_v1_resources) / sizeof(api_v1_resources[0]);
        const api_v1_resource_t *resource = &api_v1_resources[index->idx];
        api_v1_index_resource(buf, room, len, index->idx == 0, resource->path, resource->methods, resource->about);
        index->idx += 1;
        while (index->idx < count && api_v1_resources[index->idx].present != NULL &&
               !api_v1_resources[index->idx].present()) {
            index->idx += 1;
        }
        if (index->idx >= count) {
            index->phase = 3;
            index->idx = 0;
        }
        return true;
    }
    if (index->phase == 3 || index->phase == 4) {
        // A settings' path a chunk: its GET and PATCH, then its save path where it is the first to have it
        const settings_resource_t *resource = settings_resource_at(index->idx);
        if (resource == NULL) {
            index->phase = index->phase == 3 ? 4 : 5;
            index->idx = 0;
            return true;
        }
        index->idx += 1;
        if (index->phase == 3) {
            api_v1_index_resource(buf, room, len, false, resource->path, "[\"GET\",\"PATCH\"]", resource->about);
            return true;
        }
        for (size_t earlier = 0; earlier + 1 < index->idx; earlier += 1) {
            if (strcmp(settings_resource_at(earlier)->save_path, resource->save_path) == 0) {
                return true;    // stored with another: listed once
            }
        }
        // Named with every resource stored with it: the motors' save stores both
        char about[96];
        size_t used = (size_t) snprintf(about, sizeof(about), "stores %s", resource->label);
        for (size_t later = index->idx; settings_resource_at(later) != NULL && used < sizeof(about); later += 1) {
            const settings_resource_t *other = settings_resource_at(later);
            if (strcmp(other->save_path, resource->save_path) == 0) {
                used += (size_t) snprintf(about + used, sizeof(about) - used, " and %s", other->label);
            }
        }
        api_v1_index_resource(buf, room, len, false, resource->save_path, "[\"POST\"]", about);
        // Its revert beside it (ADR-008, gate 2): "stores X" becomes "puts back what is stored of X"
        char undo[112];
        snprintf(undo, sizeof(undo), "puts back what is stored of %s", about + strlen("stores "));
        api_v1_index_resource(buf, room, len, false, resource->revert_path, "[\"POST\"]", undo);
        return true;
    }
    http_json_raw(buf, room, len, "]}");
    return false;
}


/*
 * As many items as fit a chunk (ADR-008, gate 2). A path a chunk made some 50 chunks of a hundred bytes, each with
 * its own segment and its own wait for the client's ACK: 7.4 KB took 1.5 s, the schema's 34.8 KB in full chunks 0.9 s.
 * An item that does not fit is taken back and begins the next chunk
 */
static bool api_v1_index_more(void *ctx, char *buf, size_t room, size_t *len) {
    api_v1_index_t *index = ctx;
    *len = 0;
    while (true) {
        size_t mark = *len;
        api_v1_index_t before = *index;
        bool more = api_v1_index_item(index, buf, room, len);
        if (!http_json_fits(room, *len) && mark > 0) {
            *len = mark;
            buf[mark] = '\0';
            *index = before;
            return true;
        }
        if (!more) {
            return false;
        }
    }
}


static void api_v1_get_index(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    api_v1_index_t *index = http_server_reply_context(reply, sizeof(api_v1_index_t));
    memset(index, 0, sizeof(*index));
    http_server_reply_stream(reply, 200, "application/json", api_v1_index_more, index);
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


// GET /api/v1/charge_profiles: saved (the eight and the choice, one block), and each profile's id and name
// (ADR-014). Which is chosen is the charge's charge_profile
static void api_v1_get_charge_profiles(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"saved\":");
    http_json_bool(out, room, &len, charge_profiles_is_saved());
    http_json_raw(out, room, &len, ",\"profiles\":[");
    for (uint8_t idx = 0; idx < CHARGE_PROFILE_COUNT; idx += 1) {
        http_json_raw(out, room, &len, idx ? ",{\"id\":" : "{\"id\":");
        http_json_integer(out, room, &len, idx);
        http_json_raw(out, room, &len, ",\"name\":");
        http_json_string(out, room, &len, charge_profiles_get(idx)->name);
        http_json_raw(out, room, &len, "}");
    }
    http_json_raw(out, room, &len, "]}");
    if (!http_json_fits(room, len)) {
        http_server_reply_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    http_server_reply_send(reply, 200, "application/json", len);
}


// The motors' settings resources: their paths are this, then the motor's id, the name the motors' state, the events
// and the actions call it by
#define API_V1_MOTOR_SETTINGS_PREFIX "/api/v1/settings/motors/"

static bool api_v1_is_motor_settings(const settings_resource_t *resource) {
    return strncmp(resource->path, API_V1_MOTOR_SETTINGS_PREFIX, strlen(API_V1_MOTOR_SETTINGS_PREFIX)) == 0;
}


// GET /api/v1/settings/motors: saved, and each motor's id and label. Found from the resources registered, so a
// board lists the motors it carries (ADR-005). They are stored as one block, so saved is the block's, as each
// motor's own is
static void api_v1_get_motor_settings(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    bool saved = true;
    for (size_t idx = 0; settings_resource_at(idx) != NULL; idx += 1) {
        const settings_resource_t *resource = settings_resource_at(idx);
        if (api_v1_is_motor_settings(resource)) {
            saved = saved && resource->is_saved();
        }
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"saved\":");
    http_json_bool(out, room, &len, saved);
    http_json_raw(out, room, &len, ",\"motors\":[");
    bool first = true;
    for (size_t idx = 0; settings_resource_at(idx) != NULL; idx += 1) {
        const settings_resource_t *resource = settings_resource_at(idx);
        if (!api_v1_is_motor_settings(resource)) {
            continue;
        }
        http_json_raw(out, room, &len, first ? "{\"id\":" : ",{\"id\":");
        http_json_string(out, room, &len, resource->path + strlen(API_V1_MOTOR_SETTINGS_PREFIX));
        http_json_raw(out, room, &len, ",\"label\":");
        http_json_string(out, room, &len, resource->label);
        http_json_raw(out, room, &len, "}");
        first = false;
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
    api_v1_boot = chip_random32();
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1", api_v1_get_index);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/powder_profiles", api_v1_get_powder_profiles);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/charge_profiles", api_v1_get_charge_profiles);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/settings/motors", api_v1_get_motor_settings);
    // Every module's settings' table, registered at its start (gate 3)
    api_v1_settings_init();
    // The commands, the event log and the unit's reads (gate 4)
    ok = ok && api_v1_commands_init();
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/agent", api_v1_get_agent_guide);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/llms.txt", api_v1_get_agent_guide);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/display", api_v1_get_display);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/display/frame", api_v1_get_display_frame);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug", api_v1_get_debug);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug/load", api_v1_get_debug_load);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/debug/wait", api_v1_get_debug_wait);
    ok = ok && http_server_add_upload(HTTP_SERVER_PUT, "/api/v1/debug/upload", &api_v1_debug_upload);
    return ok;
}
