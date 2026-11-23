/*
 * Every throw of the charge's controller recorded (ADR-009, gate 2's rest; recording.h; api_v1.md, "Recordings").
 *
 * Two parts: the summaries with the settings in force (shared, one taken again only when one of them changed), and
 * the samples, two slots: the throw being made writes into the one that does not hold the last complete throw. A throw
 * longer than its slot is thinned: every other row dropped, then every second reading kept.
 */
#include "recording.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "activity.h"
#include "api_v1.h"
#include "http_json.h"
#include "http_server.h"
#include "profile.h"
#include "scale.h"
#include "settings.h"
#include "version.h"

/*
    What is kept (a board's build figures). The Pico 2 W keeps them in its RAM for good. The Pico W takes them from the
    FreeRTOS heap for a charge: about 50 KB free at rest, an HTTPS poll taking it down to about 5 KB and a staged
    charge's block 5.1 KB (ADR-009's progress, 2026-11-23); its samples are 8 KB, two throws of 121 rows
*/
#if PICO_RP2350
#define RECORDING_SUMMARIES 32
#define RECORDING_SNAPSHOTS 4
#define RECORDING_SLOT_ROWS 800
#else
#define RECORDING_SUMMARIES 8
#define RECORDING_SNAPSHOTS 2
#define RECORDING_SAMPLES_BYTES 8192
#endif
#define RECORDING_MARKS_MAX 16
#define RECORDING_SLOTS 2
#define RECORDING_NONE 0xFF
#define RECORDING_ROW_BATCH 4           // rows copied under the lock at a time, then written outside it

/*
    The settings a throw reads, copied whole (settings_copy_live), each in its room of a snapshot's bytes, and written
    under its name: the powder profile in use (its path made with its id), the charge's, the scale's, the motors'
*/
typedef struct {
    const char *head;           // what is written before it
    const char *path;           // NULL: the powder profile in use
    uint16_t offset;
    uint8_t room;
} recording_copy_t;

static const recording_copy_t recording_copies[] = {
    {",\"profile\":", NULL, 0, 64},
    {",\"charge\":", "/api/v1/settings/charge", 64, 64},
    {",\"scale\":", "/api/v1/settings/scale", 128, 32},
    {",\"motors\":{\"coarse\":", "/api/v1/settings/motors/coarse", 160, 80},
    {",\"fine\":", "/api/v1/settings/motors/fine", 240, 80},
};
#define RECORDING_COPY_BYTES 320
#define RECORDING_COPIES (sizeof(recording_copies) / sizeof(recording_copies[0]))

typedef struct {
    uint32_t t_ms;
    uint16_t reading;           // the reading's count in the pour, from 0
    float weight;
    float coarse_rps;
    float fine_rps;
    float forecast;
    float glide_coarse_rps;
    float glide_fine_rps;
} recording_row_t;

typedef struct {
    uint32_t t_ms;
    uint16_t reading;           // the count of the reading it fell before
    uint8_t kind;               // recording_mark_kind_t
    float value;
} recording_mark_t;

typedef struct {
    uint16_t owner;             // the recording whose samples these are; 0 for none
    uint16_t rows;
    uint16_t readings;          // readings seen in its pour
    uint16_t stride;            // a row kept every this many readings
    uint8_t thinned;            // times its rows were halved
    uint8_t mark_count;
    recording_mark_t marks[RECORDING_MARKS_MAX];
} recording_slot_t;

#ifndef RECORDING_SLOT_ROWS
#define RECORDING_SLOT_ROWS ((RECORDING_SAMPLES_BYTES - RECORDING_SLOTS * sizeof(recording_slot_t)) / \
                             (RECORDING_SLOTS * sizeof(recording_row_t)))
#endif

typedef struct {
    recording_slot_t slot[RECORDING_SLOTS];
    recording_row_t rows[RECORDING_SLOTS][RECORDING_SLOT_ROWS];
} recording_samples_t;

#ifdef RECORDING_SAMPLES_BYTES
_Static_assert(sizeof(recording_samples_t) <= RECORDING_SAMPLES_BYTES, "the samples within their budget");
#endif

#define RECORDING_COMPLETE 0x01
#define RECORDING_FIGURES 0x02          // the controller's figures are in

typedef struct {
    uint16_t id;                // the throw's number; 0: a free entry
    uint8_t snapshot;           // the settings in force, or RECORDING_NONE
    uint8_t flags;
    uint8_t result;             // event_result_t
    uint8_t reason;             // event_reason_t
    uint8_t pauses;
    uint8_t moves;
    uint8_t streams;
    uint32_t started_at_ms;     // the pour's start, on the unit's clock
    uint32_t duration_ms;
    float target;
    float weight;
    float elapsed_s;
    float first;
    float coarse_turns;
    float fine_turns;
    float stop_room;
} recording_entry_t;

typedef struct {
    uint8_t law;                // 0 the glide pours, 1 the PID law
    uint8_t simulated;
    uint8_t profile;
    uint8_t copied;             // which copies were taken: a bit each, in the order below
    uint16_t interval_ms;       // the scale's usual interval
    uint8_t data[RECORDING_COPY_BYTES];
} recording_snapshot_t;

typedef struct {
    recording_entry_t entries[RECORDING_SUMMARIES];
    recording_snapshot_t snapshots[RECORDING_SNAPSHOTS + 1];    // the last: where a new one is made, to compare
    uint8_t newest_snapshot;
} recording_summaries_t;

static SemaphoreHandle_t recording_lock;
static recording_summaries_t *recording_sums = NULL;        // NULL: nothing is recorded
static recording_samples_t *recording_samples = NULL;       // NULL: no samples
static bool recording_no_samples = false;                   // the console's: samples taken as not had
static bool recording_charge_samples = false;               // this charge's throws have samples
static uint16_t recording_open = 0;                         // the entry being made
static int8_t recording_open_slot = -1;
static uint16_t recording_last_complete = 0;                // whose samples the next pour does not write over
static uint32_t recording_pour_ms = 0;
static bool recording_poured = false;

#if PICO_RP2350
static recording_summaries_t recording_sums_kept;
static recording_samples_t recording_samples_kept;
#endif


static uint32_t recording_now_ms(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


static void recording_take(void) {
    xSemaphoreTake(recording_lock, portMAX_DELAY);
}


static void recording_give(void) {
    xSemaphoreGive(recording_lock);
}


static void recording_clear_sums(recording_summaries_t *sums) {
    memset(sums, 0, sizeof(*sums));
    sums->newest_snapshot = RECORDING_NONE;
}


static void recording_clear_samples(recording_samples_t *samples) {
    memset(samples->slot, 0, sizeof(samples->slot));
}


void recording_init(void) {
    recording_lock = xSemaphoreCreateMutex();
#if PICO_RP2350
    recording_clear_sums(&recording_sums_kept);
    recording_clear_samples(&recording_samples_kept);
    recording_sums = &recording_sums_kept;
    recording_samples = &recording_samples_kept;
#endif
}


void recording_charge_begin(void) {
#if PICO_RP2350
    // Kept across charges: a charge without samples has none of its own, and those kept before stay readable
    recording_take();
    recording_charge_samples = !recording_no_samples;
    recording_give();
#else
    // The summaries first: small, and the samples are worth nothing without them
    recording_summaries_t *sums = pvPortMalloc(sizeof(recording_summaries_t));
    recording_samples_t *samples = (sums == NULL || recording_no_samples) ? NULL :
                                   pvPortMalloc(sizeof(recording_samples_t));
    if (sums != NULL) {
        recording_clear_sums(sums);
    }
    if (samples != NULL) {
        recording_clear_samples(samples);
    }
    recording_take();
    recording_sums = sums;
    recording_samples = samples;
    recording_charge_samples = samples != NULL;
    recording_last_complete = 0;
    recording_give();
#endif
    if (recording_sums == NULL) {
        printf("Recording: none for this charge: the heap has not %u bytes (%u free)\n",
               (unsigned) sizeof(recording_summaries_t), (unsigned) xPortGetFreeHeapSize());
    }
    else if (!recording_charge_samples) {
        printf("Recording: no samples for this charge (samples_kept false): %s\n",
               recording_no_samples ? "ccrec nosamples is on" : "the heap has not the samples' bytes");
    }
}


void recording_charge_end(void) {
#if !PICO_RP2350
    recording_take();
    recording_summaries_t *sums = recording_sums;
    recording_samples_t *samples = recording_samples;
    recording_sums = NULL;
    recording_samples = NULL;
    recording_charge_samples = false;
    recording_open = 0;
    recording_open_slot = -1;
    recording_give();
    vPortFree(samples);
    vPortFree(sums);
#endif
}


static recording_entry_t *recording_find(uint16_t id) {
    if (recording_sums == NULL || id == 0) {
        return NULL;
    }
    for (uint8_t i = 0; i < RECORDING_SUMMARIES; i++) {
        if (recording_sums->entries[i].id == id) {
            return &recording_sums->entries[i];
        }
    }
    return NULL;
}


// The slot holding a recording's samples, or NULL
static recording_slot_t *recording_slot_of(uint16_t id, uint8_t *index) {
    if (recording_samples == NULL || id == 0) {
        return NULL;
    }
    for (uint8_t i = 0; i < RECORDING_SLOTS; i++) {
        if (recording_samples->slot[i].owner == id) {
            if (index != NULL) {
                *index = i;
            }
            return &recording_samples->slot[i];
        }
    }
    return NULL;
}


// A copy's path: the profile's made with its id
static const char *recording_copy_path(const recording_copy_t *copy, uint8_t profile, char *path, size_t room) {
    if (copy->path != NULL) {
        return copy->path;
    }
    snprintf(path, room, "/api/v1/powder_profiles/%u", (unsigned) profile);
    return path;
}


// The settings in force, copied into the summaries' spare snapshot. The job task's alone, so no lock of ours
static void recording_take_snapshot(recording_snapshot_t *snap, uint8_t law, bool simulated) {
    char path[40];
    memset(snap, 0, sizeof(*snap));
    snap->law = law;
    snap->simulated = simulated;
    snap->profile = (uint8_t) profile_get_selected_idx();
    snap->interval_ms = (uint16_t) scale_get_usual_interval_ms();
    activity_lock();
    for (uint8_t i = 0; i < RECORDING_COPIES; i++) {
        const recording_copy_t *copy = &recording_copies[i];
        if (settings_copy_live(recording_copy_path(copy, snap->profile, path, sizeof(path)), snap->data + copy->offset,
                               copy->room)) {
            snap->copied |= (uint8_t) (1u << i);
        }
    }
    activity_unlock();
}


uint16_t recording_throw_begin(uint16_t throw_number, float target, uint8_t law, bool simulated) {
    recording_summaries_t *sums = recording_sums;
    if (sums == NULL || throw_number == 0) {
        return 0;
    }
    recording_snapshot_t *spare = &sums->snapshots[RECORDING_SNAPSHOTS];
    recording_take_snapshot(spare, law, simulated);

    recording_take();
    // The oldest entry gives way
    recording_entry_t *entry = &sums->entries[0];
    for (uint8_t i = 1; i < RECORDING_SUMMARIES; i++) {
        if (sums->entries[i].id < entry->id) {
            entry = &sums->entries[i];
        }
    }
    entry->id = 0;
    // The settings: the newest snapshot when they are the same; else one no kept entry refers to; else none
    uint8_t snapshot = RECORDING_NONE;
    if (sums->newest_snapshot != RECORDING_NONE &&
        memcmp(&sums->snapshots[sums->newest_snapshot], spare, sizeof(*spare)) == 0) {
        snapshot = sums->newest_snapshot;
    }
    else {
        for (uint8_t s = 0; s < RECORDING_SNAPSHOTS && snapshot == RECORDING_NONE; s++) {
            bool used = false;
            for (uint8_t i = 0; i < RECORDING_SUMMARIES; i++) {
                used = used || (sums->entries[i].id != 0 && sums->entries[i].snapshot == s);
            }
            if (!used) {
                sums->snapshots[s] = *spare;
                sums->newest_snapshot = s;
                snapshot = s;
            }
        }
    }
    memset(entry, 0, sizeof(*entry));
    entry->id = throw_number;
    entry->snapshot = snapshot;
    entry->result = EVENT_RESULT_NONE;
    entry->reason = EVENT_REASON_NONE;
    entry->target = target;
    entry->weight = NAN;
    entry->elapsed_s = NAN;
    entry->first = NAN;
    recording_open = throw_number;
    recording_open_slot = -1;
    recording_poured = false;
    recording_give();
    return throw_number;
}


void recording_pour_begin(uint32_t now_ms) {
    recording_take();
    recording_entry_t *entry = recording_find(recording_open);
    if (entry != NULL) {
        entry->started_at_ms = now_ms;
        recording_pour_ms = now_ms;
        recording_poured = true;
        if (recording_samples != NULL && recording_charge_samples) {
            // Not over the last complete throw's: a client reads it while this one pours
            uint8_t s = recording_samples->slot[0].owner == recording_last_complete && recording_last_complete != 0;
            recording_slot_t *slot = &recording_samples->slot[s];
            memset(slot, 0, sizeof(*slot));
            slot->owner = recording_open;
            slot->stride = 1;
            recording_open_slot = (int8_t) s;
        }
    }
    recording_give();
}


void recording_row(uint32_t t_ms, float weight, float coarse_rps, float fine_rps, float forecast, float glide_coarse_rps,
                   float glide_fine_rps) {
    recording_take();
    if (recording_open_slot >= 0 && recording_samples != NULL) {
        recording_slot_t *slot = &recording_samples->slot[recording_open_slot];
        recording_row_t *rows = recording_samples->rows[recording_open_slot];
        uint16_t reading = slot->readings;
        slot->readings = reading < UINT16_MAX ? (uint16_t) (reading + 1) : reading;
        if (slot->rows >= RECORDING_SLOT_ROWS && reading % slot->stride == 0) {
            // Full: every other row kept, and a row every second reading from now
            uint16_t kept = 0;
            for (uint16_t i = 0; i < slot->rows; i += 2) {
                rows[kept++] = rows[i];
            }
            slot->rows = kept;
            slot->stride = (uint16_t) (slot->stride * 2);
            slot->thinned += 1;
        }
        if (reading % slot->stride == 0 && slot->rows < RECORDING_SLOT_ROWS) {
            rows[slot->rows++] = (recording_row_t) {t_ms, reading, weight, coarse_rps, fine_rps, forecast,
                                                     glide_coarse_rps, glide_fine_rps};
        }
    }
    recording_give();
}


static void recording_add_mark(recording_mark_kind_t kind, float value) {
    if (recording_open_slot < 0 || recording_samples == NULL || !recording_poured) {
        return;
    }
    recording_slot_t *slot = &recording_samples->slot[recording_open_slot];
    if (slot->mark_count < RECORDING_MARKS_MAX) {
        slot->marks[slot->mark_count++] = (recording_mark_t) {recording_now_ms() - recording_pour_ms, slot->readings,
                                                              (uint8_t) kind, value};
    }
}


void recording_mark(recording_mark_kind_t kind, float value) {
    recording_take();
    recording_entry_t *entry = recording_find(recording_open);
    if (entry != NULL) {
        if (kind == RECORDING_MARK_PAUSED && entry->pauses < UINT8_MAX) {
            entry->pauses += 1;
        }
        recording_add_mark(kind, value);
    }
    recording_give();
}


void recording_throw_controller(const recording_controller_t *figures) {
    recording_take();
    recording_entry_t *entry = recording_find(recording_open);
    if (entry != NULL) {
        entry->first = figures->first;
        entry->moves = figures->moves;
        entry->streams = figures->streams;
        entry->coarse_turns = figures->coarse_turns;
        entry->fine_turns = figures->fine_turns;
        entry->stop_room = figures->stop_room;
        entry->flags |= RECORDING_FIGURES;
    }
    recording_give();
}


void recording_throw_end(event_result_t result, event_reason_t reason, float weight, float elapsed_s) {
    recording_take();
    recording_entry_t *entry = recording_find(recording_open);
    if (entry != NULL) {
        if (result != EVENT_RESULT_NONE) {
            recording_add_mark(RECORDING_MARK_VERDICT, weight);
        }
        entry->result = (uint8_t) result;
        entry->reason = (uint8_t) reason;
        entry->weight = weight;
        entry->elapsed_s = elapsed_s;
        entry->duration_ms = recording_poured ? recording_now_ms() - recording_pour_ms : 0;
        entry->flags |= RECORDING_COMPLETE;
        if (recording_open_slot >= 0) {
            recording_last_complete = recording_open;
        }
    }
    recording_open = 0;
    recording_open_slot = -1;
    recording_poured = false;
    recording_give();
}


// --- The API

static const char *const recording_columns =
    "[\"t_ms\",\"reading\",\"weight\",\"coarse_rps\",\"fine_rps\",\"ctl_forecast\",\"ctl_glide_coarse_rps\","
    "\"ctl_glide_fine_rps\"]";

static const char *const recording_mark_names[] = {"coarse_stop", "fine_stop", "move", "paused", "resumed", "verdict"};
static const char *const recording_mark_values[] = {"weight", "weight", "turns", "pause", "weight", "weight"};

// An entry as it is, with what of its samples is kept, copied under the lock
typedef struct {
    recording_entry_t entry;
    bool kept;
    uint16_t rows;
    uint8_t thinned;
} recording_view_t;


static bool recording_view(uint16_t id, recording_view_t *view) {
    recording_take();
    const recording_entry_t *entry = recording_find(id);
    if (entry != NULL) {
        view->entry = *entry;
        const recording_slot_t *slot = recording_slot_of(id, NULL);
        view->kept = slot != NULL;
        view->rows = slot != NULL ? slot->rows : 0;
        view->thinned = slot != NULL ? slot->thinned : 0;
    }
    recording_give();
    return entry != NULL;
}


// The newest recording older than `below` (0: the newest of all); 0 when there is none
static uint16_t recording_next_below(uint16_t below) {
    uint16_t found = 0;
    recording_take();
    if (recording_sums != NULL) {
        for (uint8_t i = 0; i < RECORDING_SUMMARIES; i++) {
            uint16_t id = recording_sums->entries[i].id;
            if (id != 0 && (below == 0 || id < below) && id > found) {
                found = id;
            }
        }
    }
    recording_give();
    return found;
}


static void recording_write_float(char *out, size_t room, size_t *len, const char *name, float value) {
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
    http_json_number(out, room, len, value, 6);
}


// An entry's fields, its object left open
static void recording_write_entry(char *out, size_t room, size_t *len, const recording_view_t *view) {
    const recording_entry_t *e = &view->entry;
    bool complete = (e->flags & RECORDING_COMPLETE) != 0;
    http_json_raw(out, room, len, "{\"boot\":");
    http_json_unsigned(out, room, len, api_v1_boot_id());
    http_json_raw(out, room, len, ",\"id\":");
    http_json_integer(out, room, len, e->id);
    http_json_raw(out, room, len, ",\"kind\":\"throw\",\"job\":\"charge\",\"throw\":");
    http_json_integer(out, room, len, e->id);
    http_json_raw(out, room, len, ",\"tag\":null,\"started_at_ms\":");
    http_json_unsigned(out, room, len, e->started_at_ms);
    http_json_raw(out, room, len, ",\"duration_ms\":");
    http_json_unsigned(out, room, len, e->duration_ms);
    http_json_raw(out, room, len, ",\"rows\":");
    http_json_integer(out, room, len, view->rows);
    http_json_raw(out, room, len, ",\"complete\":");
    http_json_bool(out, room, len, complete);
    http_json_raw(out, room, len, ",\"truncated\":false,\"thinned\":");
    http_json_integer(out, room, len, view->thinned);
    http_json_raw(out, room, len, ",\"samples_kept\":");
    http_json_bool(out, room, len, view->kept);
    http_json_raw(out, room, len, ",\"summary\":{\"target\":");
    http_json_number(out, room, len, e->target, 6);
    recording_write_float(out, room, len, "weight", e->weight);
    recording_write_float(out, room, len, "error", e->weight - e->target);
    http_json_raw(out, room, len, ",\"result\":");
    http_json_string(out, room, len, events_result_name((event_result_t) e->result));
    http_json_raw(out, room, len, ",\"reason\":");
    http_json_string(out, room, len, events_reason_name((event_reason_t) e->reason));
    recording_write_float(out, room, len, "elapsed_s", e->elapsed_s);
    http_json_raw(out, room, len, ",\"pauses\":");
    http_json_integer(out, room, len, e->pauses);
    if (e->flags & RECORDING_FIGURES) {
        recording_write_float(out, room, len, "first", e->first);
        http_json_raw(out, room, len, ",\"moves\":");
        http_json_integer(out, room, len, e->moves);
        http_json_raw(out, room, len, ",\"streams\":");
        http_json_integer(out, room, len, e->streams);
        recording_write_float(out, room, len, "coarse_turns", e->coarse_turns);
        recording_write_float(out, room, len, "fine_turns", e->fine_turns);
        recording_write_float(out, room, len, "stop_room", e->stop_room);
    }
    http_json_raw(out, room, len, "}");
}


typedef struct {
    uint16_t id;                // the recording; for the list, the last written
    uint8_t phase;
    uint8_t thinned;            // the samples' at the start: a change ends the reply
    int32_t next;               // the samples: the row given last
    uint16_t mark;
} recording_reply_t;
_Static_assert(sizeof(recording_reply_t) <= HTTP_SERVER_CONTEXT_MAX, "a reply's state in its context");


// As many items as fit a chunk: an item that does not is taken back and begins the next (api_v1.c's index)
typedef bool (*recording_item_t)(recording_reply_t *reply, char *buf, size_t room, size_t *len);

static bool recording_items(recording_reply_t *reply, recording_item_t item, char *buf, size_t room, size_t *len) {
    *len = 0;
    while (true) {
        size_t mark = *len;
        recording_reply_t before = *reply;
        bool more = item(reply, buf, room, len);
        if (!http_json_fits(room, *len) && mark > 0) {
            *len = mark;
            buf[mark] = '\0';
            *reply = before;
            return true;
        }
        if (!more) {
            return false;
        }
    }
}


// GET /api/v1/recordings: newest first, each with its summary
static bool recording_list_item(recording_reply_t *reply, char *buf, size_t room, size_t *len) {
    if (reply->phase == 0) {
        http_json_raw(buf, room, len, "{\"boot\":");
        http_json_unsigned(buf, room, len, api_v1_boot_id());
        http_json_raw(buf, room, len, ",\"recordings\":[");
        reply->phase = 1;
        return true;
    }
    if (reply->phase == 1) {
        uint16_t id = recording_next_below(reply->id);
        recording_view_t view;
        if (id == 0) {
            reply->phase = 2;
            return true;
        }
        if (recording_view(id, &view)) {
            http_json_raw(buf, room, len, reply->mark ? "," : "");
            recording_write_entry(buf, room, len, &view);
            http_json_raw(buf, room, len, "}");
            reply->mark = 1;
        }
        reply->id = id;
        return true;
    }
    http_json_raw(buf, room, len, "]}");
    return false;
}


static bool recording_list_more(void *ctx, char *buf, size_t room, size_t *len) {
    return recording_items(ctx, recording_list_item, buf, room, len);
}


static void recording_get_list(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    recording_reply_t *state = http_server_reply_context(reply, sizeof(recording_reply_t));
    memset(state, 0, sizeof(*state));
    http_server_reply_stream(reply, 200, "application/json", recording_list_more, state);
}


// A snapshot's settings, from its copy taken under the lock
static void recording_write_settings(char *out, size_t room, size_t *len, const recording_snapshot_t *snap) {
    char path[40];
    http_json_raw(out, room, len, ",\"settings\":{\"firmware\":");
    http_json_string(out, room, len, version_string);
    http_json_raw(out, room, len, ",\"simulated\":");
    http_json_bool(out, room, len, snap->simulated);
    http_json_raw(out, room, len, ",\"unit\":");
    http_json_string(out, room, len, scale_get_unit());
    http_json_raw(out, room, len, ",\"controller\":\"staged\",\"law\":");
    http_json_string(out, room, len, snap->law ? "pid" : "glide");
    http_json_raw(out, room, len, ",\"powder_profile\":");
    http_json_integer(out, room, len, snap->profile);
    for (uint8_t i = 0; i < RECORDING_COPIES; i++) {
        const recording_copy_t *copy = &recording_copies[i];
        http_json_raw(out, room, len, copy->head);
        if (snap->copied & (1u << i)) {
            settings_write_copy(recording_copy_path(copy, snap->profile, path, sizeof(path)), snap->data + copy->offset,
                                out, room, len);
        }
        else {
            http_json_raw(out, room, len, "null");
        }
    }
    http_json_raw(out, room, len, "},\"scale_interval_ms\":");
    http_json_integer(out, room, len, snap->interval_ms);
    http_json_raw(out, room, len, ",\"overridden\":[]}");
}


// GET /api/v1/recordings/{id}: its entry, the settings in force, its marks
static bool recording_one_item(recording_reply_t *reply, char *buf, size_t room, size_t *len) {
    if (reply->phase == 0) {
        recording_view_t view;
        if (!recording_view(reply->id, &view)) {
            http_json_raw(buf, room, len, "{\"id\":");
            http_json_integer(buf, room, len, reply->id);
            http_json_raw(buf, room, len, ",\"gone\":true}");
            return false;
        }
        recording_write_entry(buf, room, len, &view);
        reply->phase = 1;
        return true;
    }
    if (reply->phase == 1) {
        recording_snapshot_t snap;
        bool have = false;
        recording_take();
        const recording_entry_t *entry = recording_find(reply->id);
        if (entry != NULL && entry->snapshot != RECORDING_NONE) {
            snap = recording_sums->snapshots[entry->snapshot];
            have = true;
        }
        recording_give();
        if (have) {
            recording_write_settings(buf, room, len, &snap);
        }
        else {
            http_json_raw(buf, room, len, ",\"settings\":null");
        }
        reply->phase = 2;
        return true;
    }
    if (reply->phase == 2) {
        recording_mark_t mark;
        uint16_t stride = 1;
        bool have = false;
        bool kept = false;
        recording_take();
        const recording_slot_t *slot = recording_slot_of(reply->id, NULL);
        if (slot != NULL) {
            kept = true;
            stride = slot->stride;
            if (reply->mark < slot->mark_count) {
                mark = slot->marks[reply->mark];
                have = true;
            }
        }
        recording_give();
        if (!kept) {
            http_json_raw(buf, room, len, ",\"marks\":null");
            reply->phase = 3;
            return true;
        }
        if (reply->mark == 0) {
            http_json_raw(buf, room, len, ",\"marks\":[");
        }
        if (!have) {
            http_json_raw(buf, room, len, "]");
            reply->phase = 3;
            return true;
        }
        uint8_t kind = mark.kind < sizeof(recording_mark_names) / sizeof(recording_mark_names[0]) ? mark.kind : 0;
        http_json_raw(buf, room, len, reply->mark ? ",{\"t_ms\":" : "{\"t_ms\":");
        http_json_unsigned(buf, room, len, mark.t_ms);
        http_json_raw(buf, room, len, ",\"row\":");
        http_json_integer(buf, room, len, mark.reading / stride);
        http_json_raw(buf, room, len, ",\"kind\":");
        http_json_string(buf, room, len, recording_mark_names[kind]);
        recording_write_float(buf, room, len, recording_mark_values[kind], mark.value);
        http_json_raw(buf, room, len, "}");
        reply->mark += 1;
        return true;
    }
    http_json_raw(buf, room, len, "}");
    return false;
}


static bool recording_one_more(void *ctx, char *buf, size_t room, size_t *len) {
    return recording_items(ctx, recording_one_item, buf, room, len);
}


static void recording_get_one(const http_server_request_t *request, http_server_reply_t *reply) {
    recording_view_t view;
    if (request->path_id <= 0 || request->path_id > UINT16_MAX || !recording_view((uint16_t) request->path_id, &view)) {
        http_server_reply_error(reply, 404, "not_found", "no such recording: GET /api/v1/recordings lists those kept");
        return;
    }
    recording_reply_t *state = http_server_reply_context(reply, sizeof(recording_reply_t));
    memset(state, 0, sizeof(*state));
    state->id = (uint16_t) request->path_id;
    http_server_reply_stream(reply, 200, "application/json", recording_one_more, state);
}


// GET /api/v1/recordings/{id}/samples?after=N: the rows after row N, a few copied under the lock at a time
static bool recording_samples_more(void *ctx, char *buf, size_t room, size_t *len) {
    recording_reply_t *reply = ctx;
    *len = 0;
    if (reply->phase == 0) {
        http_json_raw(buf, room, len, "{\"columns\":");
        http_json_raw(buf, room, len, recording_columns);
        http_json_raw(buf, room, len, ",\"thinned\":");
        http_json_integer(buf, room, len, reply->thinned);
        http_json_raw(buf, room, len, ",\"rows\":[");
        reply->phase = 1;
        reply->mark = 0;
        return true;
    }
    if (reply->phase == 1) {
        while (true) {
            recording_row_t batch[RECORDING_ROW_BATCH];
            uint16_t count = 0;
            bool same = false;
            recording_take();
            uint8_t s;
            const recording_slot_t *slot = recording_slot_of(reply->id, &s);
            if (slot != NULL && slot->thinned == reply->thinned) {
                same = true;
                for (int32_t row = reply->next + 1; row < slot->rows && count < RECORDING_ROW_BATCH; row++) {
                    batch[count++] = recording_samples->rows[s][row];
                }
            }
            recording_give();
            if (!same || count == 0) {
                reply->phase = 2;
                break;
            }
            // A row of eight numbers is at most about 110 characters: stop while one more surely fits
            if (*len + (size_t) count * 112 + 4 >= room) {
                return true;
            }
            for (uint16_t i = 0; i < count; i++) {
                const recording_row_t *r = &batch[i];
                http_json_raw(buf, room, len, reply->mark ? ",[" : "[");
                http_json_unsigned(buf, room, len, r->t_ms);
                http_json_raw(buf, room, len, ",");
                http_json_integer(buf, room, len, r->reading);
                const float values[] = {r->weight, r->coarse_rps, r->fine_rps, r->forecast, r->glide_coarse_rps,
                                        r->glide_fine_rps};
                for (size_t v = 0; v < sizeof(values) / sizeof(values[0]); v++) {
                    http_json_raw(buf, room, len, ",");
                    http_json_number(buf, room, len, values[v], 6);
                }
                http_json_raw(buf, room, len, "]");
                reply->mark = 1;
                reply->next += 1;
            }
        }
    }
    // Complete when the recording is and every row was given; a thinning or a slot taken since ends it short
    recording_take();
    const recording_slot_t *slot = recording_slot_of(reply->id, NULL);
    const recording_entry_t *entry = recording_find(reply->id);
    bool complete = slot != NULL && slot->thinned == reply->thinned && entry != NULL &&
               (entry->flags & RECORDING_COMPLETE) && reply->next + 1 >= slot->rows;
    recording_give();
    http_json_raw(buf, room, len, "],\"next\":");
    http_json_integer(buf, room, len, reply->next);
    http_json_raw(buf, room, len, ",\"complete\":");
    http_json_bool(buf, room, len, complete);
    http_json_raw(buf, room, len, "}");
    return false;
}


static void recording_get_samples(const http_server_request_t *request, http_server_reply_t *reply) {
    recording_view_t view;
    if (request->path_id <= 0 || request->path_id > UINT16_MAX || !recording_view((uint16_t) request->path_id, &view)) {
        http_server_reply_error(reply, 404, "not_found", "no such recording: GET /api/v1/recordings lists those kept");
        return;
    }
    if (!view.kept) {
        http_server_reply_error(reply, 410, "gone", "its samples are not kept: samples_kept is false");
        return;
    }
    char value[12];
    long after = -1;
    if (http_server_query_value(request->query, "after", value, sizeof(value)) && value[0] != '\0') {
        after = strtol(value, NULL, 10);
    }
    if (after < -1 || after > UINT16_MAX) {
        http_server_reply_error(reply, 400, "out_of_range", "after: a row's number, or -1 for all");
        return;
    }
    recording_reply_t *state = http_server_reply_context(reply, sizeof(recording_reply_t));
    memset(state, 0, sizeof(*state));
    state->id = (uint16_t) request->path_id;
    state->thinned = view.thinned;
    state->next = (int32_t) after;
    http_server_reply_stream(reply, 200, "application/json", recording_samples_more, state);
}


bool recording_api_init(void) {
    bool ok = http_server_add_route(HTTP_SERVER_GET, "/api/v1/recordings", recording_get_list);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/recordings/{id}", recording_get_one);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/recordings/{id}/samples", recording_get_samples);
    return ok;
}


// --- The console

void recording_console(const char *args, char *reply, size_t reply_size, void (*write)(const char *line, void *ctx),
                       void *write_ctx) {
    if (strncmp(args, "nosamples", 9) == 0) {
        const char *value = args + 9;
        while (*value == ' ') {
            value++;
        }
        if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0) {
            recording_no_samples = strcmp(value, "on") == 0;
            snprintf(reply, reply_size, "@ok ccrec nosamples=%u (from the next charge; never stored)",
                     recording_no_samples);
        }
        else {
            snprintf(reply, reply_size, "@err usage: ccrec [<recording>|nosamples on|off]");
        }
        return;
    }
    char *end = NULL;
    long asked = strtol(args, &end, 10);
    if (args[0] != '\0' && (end == args || *end != '\0' || asked <= 0 || asked > UINT16_MAX)) {
        snprintf(reply, reply_size, "@err usage: ccrec [<recording>|nosamples on|off]");
        return;
    }
    // The recording asked for, or the newest whose samples are kept, row by row as they are
    uint16_t id = (uint16_t) (args[0] != '\0' ? asked : 0);
    recording_take();
    if (recording_samples != NULL && id == 0) {
        for (uint8_t i = 0; i < RECORDING_SLOTS; i++) {
            if (recording_samples->slot[i].owner > id) {
                id = recording_samples->slot[i].owner;
            }
        }
    }
    recording_give();
    char line[80];
    uint16_t rows = 0;
    for (uint16_t row = 0; ; row++) {
        recording_row_t r;
        bool have = false;
        recording_take();
        uint8_t s;
        const recording_slot_t *slot = recording_slot_of(id, &s);
        if (slot != NULL && row < slot->rows) {
            r = recording_samples->rows[s][row];
            have = true;
        }
        recording_give();
        if (!have) {
            break;
        }
        snprintf(line, sizeof(line), "@r %lu %.3f %.4f %.3f %.3f", (unsigned long) r.t_ms, r.weight, r.forecast,
                 r.coarse_rps, r.fine_rps);
        write(line, write_ctx);
        rows++;
    }
    snprintf(reply, reply_size, "@ok ccrec throw=%u rows=%u", id, rows);
}
