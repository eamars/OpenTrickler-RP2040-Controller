/*
 * The update's poll (ADR-010, gate 5; update_poll.h): its stored channel and source, the poll, and the release it
 * found.
 *
 * The poll runs in a task made for it and deleted at its end, so a charge can start while it runs, and what it takes
 * from the heap is back once it ends: its stack (4 KB) and mbedTLS's 39 to 41 KB (measured on the Pico W, progress.md).
 * What it reads, the list of releases and the manifest, is read as it comes and never held whole (update_json_*).
 */
#include "update_poll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "activity.h"
#include "common.h"
#include "eeprom.h"
#include "events.h"
#include "https_client.h"
#include "settings.h"
#include "update.h"
#include "wireless.h"

#define UPDATE_POLL_TASK_STACK_WORDS 1280       // a poll used 3,352 bytes, over HTTPS or HTTP (progress.md, gate 5)
#define UPDATE_POLL_TASK_PRIORITY 2             // below the web server (3): a handshake is seconds of arithmetic
// GitHub's, or the local OTA server's in a test build that names it (-DUPDATE_TEST_SOURCE): so a bench unit's first
// start never polls GitHub's releases (the owner: tests never fetch from them)
#ifdef UPDATE_TEST_SOURCE
#define UPDATE_POLL_DEFAULT_SOURCE UPDATE_TEST_SOURCE
#else
#define UPDATE_POLL_DEFAULT_SOURCE "https://github.com/eamars/OpenTrickler-RP2040-Controller"
#endif
#define UPDATE_POLL_MANIFEST_VERSION 1          // scripts/release_manifest.py
#define UPDATE_POLL_TAG_MAX 40

static eeprom_update_config_t update_config;

static const eeprom_update_config_t default_update_config = {
    .update_config_rev = 0,
    .channel = UPDATE_CHANNEL_STABLE,           // a pre-release is offered only to a unit set to beta
    .source = UPDATE_POLL_DEFAULT_SOURCE,
};

// The poll's state, under the mutex
static SemaphoreHandle_t update_poll_mutex = NULL;
static update_poll_status_t update_poll_now = {.state = UPDATE_POLL_IDLE};
static update_poll_image_t update_poll_image;   // FOUND's
static bool update_poll_running = false;        // its task is made and not yet gone
static uint32_t update_poll_generation = 0;     // the source or the channel changed: a poll across it is dropped
static bool update_poll_start_asked = false;    // the start's poll, once


// --- JSON read as it comes: each string, number or literal handed over with its path ("images.pico_w_a.sha256",
// "[0].tag_name"). A value longer than its buffer is handed over cut; a part of a path that does not fit matches
// nothing until its place is taken by the next

#define UPDATE_JSON_DEPTH 10
#define UPDATE_JSON_PATH 64
#define UPDATE_JSON_TEXT 192

typedef void (*update_json_value_fn)(void *ctx, const char *path, const char *value, bool cut);

typedef enum {
    UPDATE_JSON_BETWEEN = 0,
    UPDATE_JSON_STRING,
    UPDATE_JSON_ESCAPE,
    UPDATE_JSON_UNICODE,
    UPDATE_JSON_BARE,
} update_json_mode_t;

typedef struct {
    update_json_value_fn value;
    void *ctx;
    char path[UPDATE_JSON_PATH];
    uint8_t base[UPDATE_JSON_DEPTH];        // the path's length where each level's own part begins
    bool array[UPDATE_JSON_DEPTH];
    uint16_t index[UPDATE_JSON_DEPTH];
    bool want_key[UPDATE_JSON_DEPTH];       // an object's: a name comes next
    int depth;                              // the levels open
    int cut_level;                          // the level whose part did not fit, -1 for none
    update_json_mode_t mode;
    bool string_is_key;
    char text[UPDATE_JSON_TEXT];
    size_t text_len;
    bool text_cut;
    uint8_t unicode_left;
    uint32_t unicode;
    bool broken;
} update_json_t;

static update_json_t update_json;   // the poll's: one at a time


static void update_json_begin(update_json_t *json, update_json_value_fn value, void *ctx) {
    memset(json, 0, sizeof(*json));
    json->value = value;
    json->ctx = ctx;
    json->cut_level = -1;
}


// A level's part of the path: an object's name, or an array's index as "[n]"
static void update_json_part(update_json_t *json, int level, const char *part, bool part_cut) {
    size_t at = json->base[level];
    json->path[at] = '\0';
    const char *dot = (at > 0 && part[0] != '[') ? "." : "";
    size_t room = sizeof(json->path) - at;
    int len = snprintf(json->path + at, room, "%s%s", dot, part);
    if (part_cut || len < 0 || (size_t) len >= room) {
        json->path[at] = '\0';
        if (json->cut_level < 0 || level < json->cut_level) {
            json->cut_level = level;
        }
    }
    else if (json->cut_level == level) {
        json->cut_level = -1;
    }
}


// A value begins: in an array, its index is its part
static void update_json_value_starts(update_json_t *json) {
    if (json->depth > 0 && json->array[json->depth - 1]) {
        char part[8];
        snprintf(part, sizeof(part), "[%u]", (unsigned) json->index[json->depth - 1]);
        update_json_part(json, json->depth - 1, part, false);
    }
}


static void update_json_emit(update_json_t *json) {
    json->text[json->text_len] = '\0';
    if (json->cut_level < 0 && !json->broken && json->depth > 0) {
        json->value(json->ctx, json->path, json->text, json->text_cut);
    }
}


static void update_json_append(update_json_t *json, char c) {
    if (json->text_len + 1 < sizeof(json->text)) {
        json->text[json->text_len++] = c;
    }
    else {
        json->text_cut = true;
    }
}


static void update_json_structural(update_json_t *json, char c) {
    switch (c) {
        case '{':
        case '[':
            update_json_value_starts(json);
            if (json->depth == UPDATE_JSON_DEPTH) {
                json->broken = true;
                return;
            }
            json->base[json->depth] = (uint8_t) strlen(json->path);
            json->array[json->depth] = c == '[';
            json->index[json->depth] = 0;
            json->want_key[json->depth] = c == '{';
            json->depth += 1;
            break;
        case '}':
        case ']':
            if (json->depth == 0) {
                json->broken = true;
                return;
            }
            json->depth -= 1;
            json->path[json->base[json->depth]] = '\0';
            if (json->cut_level >= json->depth) {
                json->cut_level = -1;
            }
            break;
        case ',':
            if (json->depth > 0) {
                if (json->array[json->depth - 1]) {
                    json->index[json->depth - 1] += 1;
                }
                else {
                    json->want_key[json->depth - 1] = true;
                }
            }
            break;
        default:    // ':'
            break;
    }
}


static void update_json_feed(update_json_t *json, const uint8_t *data, size_t len) {
    for (size_t idx = 0; idx < len && !json->broken; idx += 1) {
        char c = (char) data[idx];
        switch (json->mode) {
            case UPDATE_JSON_BETWEEN:
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    break;
                }
                if (c == '{' || c == '[' || c == '}' || c == ']' || c == ',' || c == ':') {
                    update_json_structural(json, c);
                    break;
                }
                json->text_len = 0;
                json->text_cut = false;
                if (c == '"') {
                    json->string_is_key = json->depth > 0 && !json->array[json->depth - 1] &&
                                          json->want_key[json->depth - 1];
                    if (!json->string_is_key) {
                        update_json_value_starts(json);
                    }
                    json->mode = UPDATE_JSON_STRING;
                }
                else {
                    update_json_value_starts(json);
                    update_json_append(json, c);
                    json->mode = UPDATE_JSON_BARE;
                }
                break;
            case UPDATE_JSON_STRING:
                if (c == '\\') {
                    json->mode = UPDATE_JSON_ESCAPE;
                }
                else if (c == '"') {
                    json->mode = UPDATE_JSON_BETWEEN;
                    json->text[json->text_len] = '\0';
                    if (json->string_is_key) {
                        update_json_part(json, json->depth - 1, json->text, json->text_cut);
                        json->want_key[json->depth - 1] = false;
                    }
                    else {
                        update_json_emit(json);
                    }
                }
                else {
                    update_json_append(json, c);
                }
                break;
            case UPDATE_JSON_ESCAPE:
                json->mode = UPDATE_JSON_STRING;
                if (c == 'u') {
                    json->mode = UPDATE_JSON_UNICODE;
                    json->unicode_left = 4;
                    json->unicode = 0;
                }
                else {
                    update_json_append(json, c == 'n' ? '\n' : (c == 't' ? '\t' : (c == 'r' ? '\r' :
                                             (c == 'b' || c == 'f' ? ' ' : c))));
                }
                break;
            case UPDATE_JSON_UNICODE: {
                // A character outside ASCII is a '?': the screen's fonts have none
                int digit = (c >= '0' && c <= '9') ? c - '0' : ((c | 0x20) >= 'a' && (c | 0x20) <= 'f' ?
                                                                (c | 0x20) - 'a' + 10 : -1);
                if (digit < 0) {
                    json->broken = true;
                    break;
                }
                json->unicode = (json->unicode << 4) | (uint32_t) digit;
                json->unicode_left -= 1;
                if (json->unicode_left == 0) {
                    update_json_append(json, json->unicode < 0x80 ? (char) json->unicode : '?');
                    json->mode = UPDATE_JSON_STRING;
                }
                break;
            }
            case UPDATE_JSON_BARE:
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '}' || c == ']') {
                    update_json_emit(json);
                    json->mode = UPDATE_JSON_BETWEEN;
                    if (c == ',' || c == '}' || c == ']') {
                        update_json_structural(json, c);
                    }
                }
                else {
                    update_json_append(json, c);
                }
                break;
        }
    }
}


// --- What a poll reads

typedef struct {
    int status;                         // the last fetch's HTTP status
    // The list of releases (beta)
    char tag[UPDATE_POLL_TAG_MAX];
    // The manifest
    char image_prefix[40];              // "images.<takes>."
    long manifest_version;
    char version[24];
    char note[UPDATE_NOTE_MAX];
    char importance[16];
    char url[256];
    char file[96];
    long size;
    char sha256[65];
    char block[24];
    bool url_cut;
} update_poll_read_t;

static update_poll_read_t update_poll_read;     // the poll task's own
static char update_poll_url[256];               // the poll task's own: the address it fetches


static bool update_poll_sink_begin(void *ctx, int status, long length) {
    (void) length;
    update_poll_read_t *read = ctx;
    read->status = status;
    // Only a 200's body is read
    return status == 200;
}


static bool update_poll_sink_piece(void *ctx, const uint8_t *data, size_t len) {
    (void) ctx;
    update_json_feed(&update_json, data, len);
    return !update_json.broken;
}


static void update_poll_copy(char *out, size_t room, const char *value) {
    snprintf(out, room, "%s", value);
}


// The list of releases, newest first: the first one's tag
static void update_poll_list_value(void *ctx, const char *path, const char *value, bool cut) {
    update_poll_read_t *read = ctx;
    if (strcmp(path, "[0].tag_name") == 0 && !cut) {
        update_poll_copy(read->tag, sizeof(read->tag), value);
    }
}


static void update_poll_manifest_value(void *ctx, const char *path, const char *value, bool cut) {
    update_poll_read_t *read = ctx;
    if (strcmp(path, "manifest_version") == 0) {
        read->manifest_version = strtol(value, NULL, 10);
    }
    else if (strcmp(path, "version") == 0 && !cut) {
        update_poll_copy(read->version, sizeof(read->version), value);
    }
    else if (strcmp(path, "note") == 0) {
        update_poll_copy(read->note, sizeof(read->note), value);
    }
    else if (strcmp(path, "importance") == 0 && !cut) {
        update_poll_copy(read->importance, sizeof(read->importance), value);
    }
    else if (strncmp(path, read->image_prefix, strlen(read->image_prefix)) == 0) {
        const char *name = path + strlen(read->image_prefix);
        if (strcmp(name, "url") == 0) {
            update_poll_copy(read->url, sizeof(read->url), value);
            read->url_cut = cut || strlen(value) >= sizeof(read->url);
        }
        else if (strcmp(name, "file") == 0 && !cut) {
            update_poll_copy(read->file, sizeof(read->file), value);
        }
        else if (strcmp(name, "size") == 0) {
            read->size = strtol(value, NULL, 10);
        }
        else if (strcmp(name, "sha256") == 0 && !cut) {
            update_poll_copy(read->sha256, sizeof(read->sha256), value);
        }
        else if (strcmp(name, "block") == 0 && !cut) {
            update_poll_copy(read->block, sizeof(read->block), value);
        }
    }
}


// One fetch read as JSON as it comes. False with why when it could not be fetched or read; a status other than 200
// is in read->status, for the caller to judge
static bool update_poll_fetch_json(const char *url, update_json_value_fn value, char *why, size_t why_size) {
    update_poll_read.status = 0;
    update_json_begin(&update_json, value, &update_poll_read);
    https_sink_t sink = {.begin = update_poll_sink_begin, .piece = update_poll_sink_piece, .ctx = &update_poll_read};
    bool ok = https_get(url, &sink);
    if (update_poll_read.status != 0 && update_poll_read.status != 200) {
        return true;
    }
    if (!ok) {
        https_report_t report;
        https_last_report(&report);
        snprintf(why, why_size, "%s", report.error[0] != '\0' ? report.error : "the fetch failed");
        return false;
    }
    if (update_json.broken || update_json.depth != 0) {
        snprintf(why, why_size, "not JSON");
        return false;
    }
    return true;
}


// The source split: "https://github.com" and "/eamars/OpenTrickler-RP2040-Controller". False when it is not an
// http or https address with a host
static bool update_poll_split_source(const char *source, char *origin, size_t origin_size, const char **path) {
    const char *after = strstr(source, "://");
    if (after == NULL || (strncmp(source, "http://", 7) != 0 && strncmp(source, "https://", 8) != 0)) {
        return false;
    }
    const char *slash = strchr(after + 3, '/');
    size_t origin_len = slash != NULL ? (size_t) (slash - source) : strlen(source);
    if (origin_len <= (size_t) (after + 3 - source) || origin_len >= origin_size) {
        return false;
    }
    memcpy(origin, source, origin_len);
    origin[origin_len] = '\0';
    *path = slash != NULL ? slash : "";
    return true;
}


static uint8_t update_poll_hex(char c) {
    return (uint8_t) ((c >= '0' && c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10);
}


static bool update_poll_sha256(const char *hex, uint8_t out[32]) {
    if (strlen(hex) != 64) {
        return false;
    }
    for (size_t idx = 0; idx < 64; idx += 1) {
        char c = hex[idx];
        if (!((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'))) {
            return false;
        }
    }
    for (size_t idx = 0; idx < 32; idx += 1) {
        out[idx] = (uint8_t) (update_poll_hex(hex[2 * idx]) << 4 | update_poll_hex(hex[2 * idx + 1]));
    }
    return true;
}


// The poll's end, kept unless the source or the channel changed while it ran
static void update_poll_finish(uint32_t generation, update_poll_state_t state, const char *why,
                               const update_poll_image_t *image, update_channel_t channel) {
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    if (generation != update_poll_generation) {
        update_poll_now.state = UPDATE_POLL_IDLE;
    }
    else {
        update_poll_now.state = state;
        update_poll_now.channel = channel;
        snprintf(update_poll_now.error, sizeof(update_poll_now.error), "%s", why != NULL ? why : "");
        if (state == UPDATE_POLL_FOUND) {
            update_poll_image = *image;
            snprintf(update_poll_now.version, sizeof(update_poll_now.version), "%s", update_poll_read.version);
            snprintf(update_poll_now.note, sizeof(update_poll_now.note), "%s", update_poll_read.note);
            update_poll_now.block = image->block;
            update_poll_now.critical = strcmp(update_poll_read.importance, "critical") == 0;
        }
    }
    xSemaphoreGive(update_poll_mutex);
    if (state == UPDATE_POLL_FAILED) {
        printf("Update: the poll failed: %s\n", why);
    }
    else if (state == UPDATE_POLL_FOUND) {
        printf("Update: %s found, offered\n", update_poll_read.version);
        event_t event = {.source = EVENT_SOURCE_UPDATE, .kind = EVENT_FOUND, .by = ACTIVITY_BY_UNIT,
                         .version = image->block};
        events_add(&event);
    }
    else {
        printf("Update: up to date\n");
    }
}


static void update_poll_run(void) {
    // What it asks of, as it was when it began
    eeprom_update_config_t config;
    activity_lock();
    config = update_config;
    activity_unlock();
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    uint32_t generation = update_poll_generation;
    xSemaphoreGive(update_poll_mutex);
    update_channel_t channel = config.channel == UPDATE_CHANNEL_BETA ? UPDATE_CHANNEL_BETA : UPDATE_CHANNEL_STABLE;

    char why[64] = "";
    char origin[UPDATE_SOURCE_MAX];
    const char *path;
    size_t source_len = strlen(config.source);
    while (source_len > 0 && config.source[source_len - 1] == '/') {
        config.source[--source_len] = '\0';
    }
    if (!update_poll_split_source(config.source, origin, sizeof(origin), &path)) {
        update_poll_finish(generation, UPDATE_POLL_FAILED, "the source is no http or https address", NULL, channel);
        return;
    }
    memset(&update_poll_read, 0, sizeof(update_poll_read));
    update_status_t running;
    update_status(&running);
    snprintf(update_poll_read.image_prefix, sizeof(update_poll_read.image_prefix), "images.%s.", running.takes);

    // Beta: the newest release's tag, from the list of releases
    if (channel == UPDATE_CHANNEL_BETA) {
        if (strcmp(origin, "https://github.com") == 0) {
            snprintf(update_poll_url, sizeof(update_poll_url), "https://api.github.com/repos%s/releases?per_page=1",
                     path);
        }
        else {
            snprintf(update_poll_url, sizeof(update_poll_url), "%s/api/v3/repos%s/releases?per_page=1", origin,
                     path);
        }
        if (!update_poll_fetch_json(update_poll_url, update_poll_list_value, why, sizeof(why))) {
            update_poll_finish(generation, UPDATE_POLL_FAILED, why, NULL, channel);
            return;
        }
        if (update_poll_read.status != 200) {
            snprintf(why, sizeof(why), "the list of releases answered %d", update_poll_read.status);
            update_poll_finish(generation, UPDATE_POLL_FAILED, why, NULL, channel);
            return;
        }
        if (update_poll_read.tag[0] == '\0') {
            update_poll_finish(generation, UPDATE_POLL_UP_TO_DATE, NULL, NULL, channel);
            return;
        }
        snprintf(update_poll_url, sizeof(update_poll_url), "%s/releases/download/%s/manifest.json", config.source,
                 update_poll_read.tag);
    }
    else {
        snprintf(update_poll_url, sizeof(update_poll_url), "%s/releases/latest/download/manifest.json",
                 config.source);
    }

    // The manifest. None on stable is no stable release yet: nothing to offer
    if (!update_poll_fetch_json(update_poll_url, update_poll_manifest_value, why, sizeof(why))) {
        update_poll_finish(generation, UPDATE_POLL_FAILED, why, NULL, channel);
        return;
    }
    if (update_poll_read.status == 404 && channel == UPDATE_CHANNEL_STABLE) {
        update_poll_finish(generation, UPDATE_POLL_UP_TO_DATE, NULL, NULL, channel);
        return;
    }
    if (update_poll_read.status != 200) {
        snprintf(why, sizeof(why), "the manifest answered %d", update_poll_read.status);
        update_poll_finish(generation, UPDATE_POLL_FAILED, why, NULL, channel);
        return;
    }
    update_poll_image_t image = {0};
    if (update_poll_read.manifest_version != UPDATE_POLL_MANIFEST_VERSION) {
        update_poll_finish(generation, UPDATE_POLL_FAILED, "a manifest of another version", NULL, channel);
        return;
    }
    // A release no newer than the image running is nothing to offer, whatever images it carries. No downgrade
    // (ADR-004): only a newer one is offered
    uint32_t release;
    if (update_version_parse(update_poll_read.version, &release) && release <= update_running_block()) {
        update_poll_finish(generation, UPDATE_POLL_UP_TO_DATE, NULL, NULL, channel);
        return;
    }
    if (update_poll_read.block[0] == '\0' || update_poll_read.file[0] == '\0') {
        snprintf(why, sizeof(why), "the release has no image %s", running.takes);
        update_poll_finish(generation, UPDATE_POLL_FAILED, why, NULL, channel);
        return;
    }
    if (!update_version_parse(update_poll_read.block, &image.block) || update_poll_read.size <= 0 ||
        !update_poll_sha256(update_poll_read.sha256, image.sha256) || update_poll_read.url_cut) {
        update_poll_finish(generation, UPDATE_POLL_FAILED, "the manifest's image is not whole", NULL, channel);
        return;
    }
    // Its image's own version decides, as the unit's check of the image does
    if (image.block <= update_running_block()) {
        update_poll_finish(generation, UPDATE_POLL_UP_TO_DATE, NULL, NULL, channel);
        return;
    }
    image.size = (uint32_t) update_poll_read.size;
    if (update_poll_read.url[0] != '\0') {
        snprintf(image.url, sizeof(image.url), "%s", update_poll_read.url);
    }
    else {
        // A manifest made without addresses: the file beside it, in its release by its tag
        int len;
        if (update_poll_read.tag[0] != '\0') {
            len = snprintf(image.url, sizeof(image.url), "%s/releases/download/%s/%s", config.source,
                           update_poll_read.tag, update_poll_read.file);
        }
        else {
            len = snprintf(image.url, sizeof(image.url), "%s/releases/download/v%s/%s", config.source,
                           update_poll_read.version, update_poll_read.file);
        }
        if (len < 0 || (size_t) len >= sizeof(image.url)) {
            update_poll_finish(generation, UPDATE_POLL_FAILED, "the image's address is too long", NULL, channel);
            return;
        }
    }
    update_poll_finish(generation, UPDATE_POLL_FOUND, NULL, &image, channel);
}


static void update_poll_task(void *p) {
    (void) p;
    update_poll_run();
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    update_poll_running = false;
    xSemaphoreGive(update_poll_mutex);
    vTaskDelete(NULL);
}


// --- Asking, and what was found

static bool update_poll_on_station(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return strcmp(info.mode, "station") == 0 && info.address[0] != '\0';
}


update_poll_ask_t update_poll_ask(void) {
    if (!update_poll_on_station()) {
        return UPDATE_POLL_NO_NETWORK;
    }
    // The update's job fetches its image with the same client
    if (activity_get() == ACTIVITY_UPDATE) {
        return UPDATE_POLL_BUSY;
    }
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    if (update_poll_running) {
        xSemaphoreGive(update_poll_mutex);
        return UPDATE_POLL_BUSY;
    }
    update_poll_running = true;
    update_poll_now.state = UPDATE_POLL_CHECKING;
    update_poll_now.error[0] = '\0';
    xSemaphoreGive(update_poll_mutex);
    if (xTaskCreate(update_poll_task, "Update poll", UPDATE_POLL_TASK_STACK_WORDS, NULL, UPDATE_POLL_TASK_PRIORITY,
                    NULL) != pdPASS) {
        xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
        update_poll_running = false;
        update_poll_now.state = UPDATE_POLL_FAILED;
        snprintf(update_poll_now.error, sizeof(update_poll_now.error), "no room for the poll's task");
        xSemaphoreGive(update_poll_mutex);
    }
    return UPDATE_POLL_ASKED;
}


void update_poll_station_joined(void) {
    // Once each start, quietly, the first time the station is on its network (menu_design.md)
    if (update_poll_start_asked) {
        return;
    }
    update_poll_start_asked = true;
    update_poll_ask();
}


void update_poll_status(update_poll_status_t *status) {
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    *status = update_poll_now;
    xSemaphoreGive(update_poll_mutex);
}


bool update_poll_found_image(update_poll_image_t *image) {
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    bool found = update_poll_now.state == UPDATE_POLL_FOUND;
    if (found) {
        *image = update_poll_image;
    }
    xSemaphoreGive(update_poll_mutex);
    return found;
}


bool update_poll_console(char *reply, size_t reply_size) {
    update_poll_status_t status;
    update_poll_status(&status);
    https_report_t report;
    https_last_report(&report);
    static const char *const states[] = {"idle", "checking", "up_to_date", "found", "failed"};
    snprintf(reply, reply_size,
             "@ok update poll state=%s channel=%s source=%s version=%s error=\"%s\" fetch_status=%d bytes=%lu hops=%u "
             "handshake_ms=%lu,%lu,%lu total_ms=%lu mbedtls_peak=%lu heap_before=%lu heap_min=%lu stack_left=%lu "
             "fetch_error=\"%s\"",
             states[status.state], update_config.channel == UPDATE_CHANNEL_BETA ? "beta" : "stable",
             update_config.source, status.state == UPDATE_POLL_FOUND ? status.version : "", status.error,
             report.status, (unsigned long) report.bytes, (unsigned) report.hops,
             (unsigned long) report.handshake_ms[0], (unsigned long) report.handshake_ms[1],
             (unsigned long) report.handshake_ms[2], (unsigned long) report.total_ms,
             (unsigned long) report.mbedtls_peak, (unsigned long) report.heap_free_before,
             (unsigned long) report.heap_min_after, (unsigned long) report.stack_left, report.error);
    return true;
}


// --- The stored channel and source: /api/v1/settings/update

static const setting_choice_t update_channel_choices[] = {
    {.held = UPDATE_CHANNEL_STABLE, .text = "stable", .label = "Stable"},
    {.held = UPDATE_CHANNEL_BETA, .text = "beta", .label = "Beta: pre-releases too"},
};

#define UPDATE_FIELD(member) SETTING_AT(eeprom_update_config_t, member)

static const setting_field_t update_fields[] = {
    {.name = "channel", .label = "Channel", .group = "Update",
     .about = "Which releases are offered: stable ones, or the newest of all with pre-releases (beta). Leaving beta "
              "goes back to no version: nothing is offered until a stable release is newer than the one running",
     .type = SETTING_CHOICE, UPDATE_FIELD(channel), SETTING_CHOICES(update_channel_choices), .applies = "now"},
    {.name = "source", .label = "Source", .group = "Update",
     .about = "Where the releases are: a repository's address, its releases laid out under it as GitHub lays them "
              "out (releases/latest/download/manifest.json). GitHub's own by default",
     .type = SETTING_STRING, UPDATE_FIELD(source), .length_min = 8, .applies = "now"},
};


static void *update_settings_live(int32_t id) {
    (void) id;
    return &update_config;
}


static const char *update_settings_check(const void *proposed, int32_t id, const char **field) {
    (void) id;
    const eeprom_update_config_t *next = proposed;
    char origin[UPDATE_SOURCE_MAX];
    const char *path;
    if (!update_poll_split_source(next->source, origin, sizeof(origin), &path)) {
        *field = "source";
        return "an http:// or https:// address with a host";
    }
    return NULL;
}


// A new source or channel: what was found is forgotten, and a poll across the change is dropped
static void update_settings_apply(const void *before, int32_t id) {
    (void) id;
    const eeprom_update_config_t *was = before;
    if (was->channel == update_config.channel && strcmp(was->source, update_config.source) == 0) {
        return;
    }
    xSemaphoreTake(update_poll_mutex, portMAX_DELAY);
    update_poll_generation += 1;
    if (update_poll_now.state != UPDATE_POLL_CHECKING) {
        update_poll_now.state = UPDATE_POLL_IDLE;
    }
    xSemaphoreGive(update_poll_mutex);
}


static bool update_settings_is_saved(void) {
    return config_is_saved(EEPROM_UPDATE_CONFIG_BASE_ADDR, &update_config, sizeof(update_config));
}


static const settings_resource_t update_settings = {
    .path = "/api/v1/settings/update",
    .save_path = "/api/v1/settings/update/save",
    .label = "Update",
    .about = "Where the unit looks for a firmware update, and which releases it offers",
    SETTING_FIELDS(update_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(eeprom_update_config_t),
    .live = update_settings_live,
    .check = update_settings_check,
    .apply = update_settings_apply,
    .save = update_poll_config_save,
    .is_saved = update_settings_is_saved,
};


bool update_poll_config_save(void) {
    return save_config(EEPROM_UPDATE_CONFIG_BASE_ADDR, &update_config, sizeof(eeprom_update_config_t));
}


bool update_poll_init(void) {
    update_poll_mutex = xSemaphoreCreateMutex();
    bool is_ok = update_poll_mutex != NULL && https_client_init();
    if (!load_config(EEPROM_UPDATE_CONFIG_BASE_ADDR, &update_config, &default_update_config,
                     sizeof(eeprom_update_config_t), EEPROM_UPDATE_CONFIG_REV)) {
        printf("Update: the stored source could not be read; GitHub's is used\n");
        update_config = default_update_config;
    }
    // A string read back is ended, whatever the slot held
    update_config.source[sizeof(update_config.source) - 1] = '\0';
    eeprom_register_handler(update_poll_config_save);
    settings_register(&update_settings);
    return is_ok;
}
