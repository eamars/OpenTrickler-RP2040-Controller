/*
 * One event log for the unit (ADR-003, gate 4); see events.h.
 */
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include "pico/time.h"

#include "activity.h"
#include "events.h"
#include "http_json.h"
#include "update.h"
#include "version.h"

static event_t events_ring[EVENTS_KEPT];
static uint32_t events_latest = 0;     // the newest's number; the ring holds latest - EVENTS_KEPT + 1 to latest
static SemaphoreHandle_t events_mutex = NULL;


void events_init(void) {
    if (events_mutex == NULL) {
        events_mutex = xSemaphoreCreateMutex();
        configASSERT(events_mutex != NULL);
    }
    event_t started = {.source = EVENT_SOURCE_SYSTEM, .kind = EVENT_STARTED, .text = version_string};
    events_add(&started);
}


void events_add(const event_t *event) {
    xSemaphoreTake(events_mutex, portMAX_DELAY);
    events_latest += 1;
    event_t *slot = &events_ring[events_latest % EVENTS_KEPT];
    *slot = *event;
    slot->seq = events_latest;
    slot->at_ms = time_us_64() / 1000u;
    xSemaphoreGive(events_mutex);
}


uint32_t events_latest_seq(void) {
    return events_latest;
}


uint32_t events_first_seq(void) {
    uint32_t latest = events_latest;
    return latest >= EVENTS_KEPT ? latest - EVENTS_KEPT + 1 : (latest > 0 ? 1 : 0);
}


bool events_get(uint32_t seq, event_t *event) {
    xSemaphoreTake(events_mutex, portMAX_DELAY);
    uint32_t latest = events_latest;
    bool kept = seq >= 1 && seq <= latest && latest - seq < EVENTS_KEPT;
    if (kept) {
        *event = events_ring[seq % EVENTS_KEPT];
    }
    xSemaphoreGive(events_mutex);
    return kept;
}


static const char *source_name(uint8_t source) {
    static const char *const names[] = {"charge", "throw", "purge", "motors", "system", "update", "setting", "setup",
                                        "wifi", "calibration"};
    return source < sizeof(names) / sizeof(names[0]) ? names[source] : "other";
}


static const char *kind_name(uint8_t kind) {
    static const char *const names[] = {"started", "stopped", "paused", "resumed", "finished", "ended", "fault",
                                        "cleared", "received", "installing", "confirmed", "back", "found", "changed",
                                        "stored", "step", "joining", "joined", "not_joined"};
    return kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "other";
}


const char *events_reason_name(event_reason_t reason) {
    switch (reason) {
        case EVENT_REASON_STOPPED: return "stopped";
        case EVENT_REASON_FAULT: return "fault";
        case EVENT_REASON_TIME: return "time";
        case EVENT_REASON_FINISHED: return "finished";
        case EVENT_REASON_SCALE_NO_READING: return "scale_no_reading";
        case EVENT_REASON_SCALE_UNSTEADY: return "scale_unsteady";
        case EVENT_REASON_SCALE_UNRELIABLE: return "scale_unreliable";
        case EVENT_REASON_DRIVER_FAULT: return "driver_fault";
        case EVENT_REASON_REFUSED: return "refused";
        case EVENT_REASON_DID_NOT_START: return "did_not_start";
        case EVENT_REASON_NO_NETWORK: return "no_network";
        case EVENT_REASON_NOT_CONFIRMED: return "not_confirmed";
        case EVENT_REASON_POWER_CUT: return "power_cut";
        case EVENT_REASON_WRONG_PASSWORD: return "wrong_password";
        case EVENT_REASON_TIMEOUT: return "timeout";
        case EVENT_REASON_FAILED: return "failed";
        case EVENT_REASON_CUP_LIFTED: return "cup_lifted";
        case EVENT_REASON_NONE:
        default: return "none";
    }
}


const char *events_result_name(event_result_t result) {
    switch (result) {
        case EVENT_RESULT_GOOD: return "good";
        case EVENT_RESULT_UNDER: return "under";
        case EVENT_RESULT_OVER: return "over";
        case EVENT_RESULT_NONE:
        default: return "none";
    }
}


static void write_member(char *out, size_t room, size_t *len, const char *name) {
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
}


static const char *motor_name(uint8_t motor) {
    return motor == 0 ? "coarse" : "fine";
}


void events_write_json(const event_t *event, char *out, size_t room, size_t *len) {
    http_json_raw(out, room, len, "{\"seq\":");
    http_json_integer(out, room, len, (long) event->seq);
    write_member(out, room, len, "at_ms");
    // A 64-bit clock, past 32 bits after 49 days; its digits written here, not by the SDK's printf
    char digits[24];
    size_t pos = sizeof(digits) - 1;
    digits[pos] = '\0';
    uint64_t at_ms = event->at_ms;
    do {
        digits[--pos] = (char) ('0' + at_ms % 10);
        at_ms /= 10;
    } while (at_ms > 0 && pos > 0);
    http_json_raw(out, room, len, digits + pos);
    write_member(out, room, len, "source");
    http_json_string(out, room, len, source_name(event->source));
    write_member(out, room, len, "kind");
    http_json_string(out, room, len, kind_name(event->kind));

    // What each kind carries (api_v1.md, the kinds' table)
    switch (event->source) {
        case EVENT_SOURCE_CHARGE:
        case EVENT_SOURCE_CLEANUP:
            write_member(out, room, len, "by");
            http_json_string(out, room, len, activity_by_name((activity_by_t) event->by));
            if (event->kind == EVENT_STARTED && event->source == EVENT_SOURCE_CHARGE) {
                write_member(out, room, len, "target");
                http_json_float(out, room, len, event->value);
                write_member(out, room, len, "powder_profile");
                http_json_integer(out, room, len, event->number);
            }
            if (event->kind == EVENT_STOPPED) {
                write_member(out, room, len, "reason");
                http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
            }
            break;
        case EVENT_SOURCE_THROW:
            write_member(out, room, len, "throw");
            http_json_integer(out, room, len, event->throw_number);
            if (event->kind == EVENT_STARTED) {
                write_member(out, room, len, "target");
                http_json_float(out, room, len, event->value);
                write_member(out, room, len, "powder_profile");
                http_json_integer(out, room, len, event->number);
            }
            else if (event->kind == EVENT_PAUSED) {
                write_member(out, room, len, "pause");
                http_json_integer(out, room, len, event->number);
            }
            else if (event->kind == EVENT_RESUMED) {
                write_member(out, room, len, "weight");
                http_json_float(out, room, len, event->value);
            }
            else if (event->kind == EVENT_FINISHED) {
                write_member(out, room, len, "result");
                http_json_string(out, room, len, events_result_name((event_result_t) event->result));
                write_member(out, room, len, "weight");
                http_json_float(out, room, len, event->value);
                write_member(out, room, len, "elapsed_s");
                http_json_float(out, room, len, event->value2);
            }
            else if (event->kind == EVENT_ENDED) {
                write_member(out, room, len, "reason");
                http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
                if (event->reason == EVENT_REASON_DRIVER_FAULT) {
                    write_member(out, room, len, "motor");
                    http_json_string(out, room, len, motor_name(event->number));
                    write_member(out, room, len, "fault");
                    http_json_string(out, room, len, event->text != NULL ? event->text : "fault");
                }
            }
            break;
        case EVENT_SOURCE_CALIBRATION:
            // Its part; who started or stopped it, and why it stopped
            write_member(out, room, len, "part");
            http_json_string(out, room, len, event->text != NULL ? event->text : "none");
            if (event->kind == EVENT_STARTED || event->kind == EVENT_STOPPED) {
                write_member(out, room, len, "by");
                http_json_string(out, room, len, activity_by_name((activity_by_t) event->by));
            }
            if (event->kind == EVENT_STOPPED) {
                write_member(out, room, len, "reason");
                http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
            }
            break;
        case EVENT_SOURCE_MOTORS:
            write_member(out, room, len, "motor");
            http_json_string(out, room, len, motor_name(event->number));
            write_member(out, room, len, "fault");
            http_json_string(out, room, len, event->text != NULL ? event->text : "fault");
            break;
        case EVENT_SOURCE_UPDATE:
            // started and stopped: who; a stop's reason, and a refusal's code. The others: the version tried
            if (event->kind == EVENT_STARTED || event->kind == EVENT_STOPPED) {
                write_member(out, room, len, "by");
                http_json_string(out, room, len, activity_by_name((activity_by_t) event->by));
            }
            if (event->kind == EVENT_STOPPED) {
                write_member(out, room, len, "reason");
                http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
                if (event->reason == EVENT_REASON_REFUSED) {
                    write_member(out, room, len, "code");
                    http_json_string(out, room, len, event->text != NULL ? event->text : "");
                }
            } else if (event->kind != EVENT_STARTED) {
                char version[16] = "";
                if (event->version != 0) {
                    update_version_text(event->version, version, sizeof(version));
                }
                write_member(out, room, len, "version");
                http_json_string(out, room, len, version);
                if (event->kind == EVENT_BACK) {
                    write_member(out, room, len, "reason");
                    http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
                }
                if (event->kind == EVENT_CONFIRMED) {
                    // The settings the update reset, by name (ADR-008, gate 2): a stored layout that changed
                    write_member(out, room, len, "reset");
                    update_write_reset(event->throw_number, out, room, len);
                }
            }
            break;
        case EVENT_SOURCE_SETTING: {
            // Who wrote it, and the resource's path, or the save's for a store: a page reads that resource again
            write_member(out, room, len, "by");
            http_json_string(out, room, len, activity_by_name((activity_by_t) event->by));
            char path[72];
            const char *template = event->text != NULL ? event->text : "";
            const char *id = strstr(template, "{id}");
            if (id != NULL) {
                snprintf(path, sizeof(path), "%.*s%u", (int) (id - template), template, (unsigned) event->number);
            }
            else {
                snprintf(path, sizeof(path), "%s", template);
            }
            write_member(out, room, len, "path");
            http_json_string(out, room, len, path);
            break;
        }
        case EVENT_SOURCE_SYSTEM:
            write_member(out, room, len, "firmware");
            http_json_string(out, room, len, event->text != NULL ? event->text : "");
            break;
        case EVENT_SOURCE_SETUP:
            // The step it went on to, and who asked: a page and the menu read GET /api/v1/setup again
            write_member(out, room, len, "step");
            http_json_string(out, room, len, event->text != NULL ? event->text : "");
            write_member(out, room, len, "by");
            http_json_string(out, room, len, activity_by_name((activity_by_t) event->by));
            break;
        case EVENT_SOURCE_WIFI:
            // The network is in GET /api/v1/network's station: a name is not a string the log may keep
            if (event->kind == EVENT_NOT_JOINED) {
                write_member(out, room, len, "reason");
                http_json_string(out, room, len, events_reason_name((event_reason_t) event->reason));
            }
            break;
        default:
            break;
    }
    http_json_raw(out, room, len, "}");
}
