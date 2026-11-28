/*
 * The throw record (ADR-016, gate 1); see charge_throws.h.
 */
#include <math.h>
#include <string.h>

#include <FreeRTOS.h>

#include "chip.h"
#include "rtos_port.h"

#include "charge_throws.h"
#include "events.h"
#include "http_json.h"

typedef struct {
    charge_throw_t kept[CHARGE_THROWS_KEPT];    // by number: kept[number % CHARGE_THROWS_KEPT]
    charge_throw_t now;                         // the open one, the charge's task's alone
    uint16_t newest;                            // the newest kept's number; 0, none
    uint16_t count;                             // how many are kept
    bool open;
} charge_throws_room_t;

static charge_throws_room_t *throws = NULL;


bool charge_throws_ready(void) {
    if (throws == NULL) {
        throws = (charge_throws_room_t *) pvPortMalloc(sizeof(charge_throws_room_t));
        if (throws != NULL) {
            memset(throws, 0, sizeof(*throws));
        }
    }
    return throws != NULL;
}


charge_throw_t *charge_throws_open(uint16_t number, uint8_t charge_profile, uint8_t powder_profile, float target,
                                   bool simulated) {
    if (throws == NULL) {
        return NULL;
    }
    // Every figure not reached yet
    static const charge_throw_t blank = {
        .weight = NAN, .elapsed_s = NAN, .coarse_s = NAN, .fine_s = NAN,
        .coarse_stop_weight = NAN, .coarse_stop_rps = NAN, .fine_stop_weight = NAN, .fine_stop_rps = NAN,
        .coarse_kp = NAN, .coarse_ki = NAN, .coarse_kd = NAN, .fine_kp = NAN, .fine_ki = NAN, .fine_kd = NAN,
        .coarse_threshold = NAN, .fine_threshold = NAN,
        .result = (uint8_t) EVENT_RESULT_NONE, .reason = (uint8_t) EVENT_REASON_NONE,
    };
    charge_throw_t *record = &throws->now;
    *record = blank;
    record->number = number;
    record->charge_profile = charge_profile;
    record->powder_profile = powder_profile;
    record->target = target;
    record->simulated = simulated;
    throws->open = true;
    return record;
}


charge_throw_t *charge_throws_current(void) {
    return throws != NULL && throws->open ? &throws->now : NULL;
}


void charge_throws_close(uint8_t result, uint8_t reason, float weight) {
    if (throws == NULL || !throws->open) {
        return;
    }
    throws->open = false;
    throws->now.result = result;
    throws->now.reason = reason;
    throws->now.weight = weight;
    throws->now.at_ms = chip_time_us() / 1000u;
    // Kept under a critical section: a reader in another task copies a whole record, never half of one
    rtos_critical_enter();
    throws->kept[throws->now.number % CHARGE_THROWS_KEPT] = throws->now;
    // Numbered from 1 at each start, one after another: a number below the newest is a new start's
    if (throws->count == 0 || throws->now.number != throws->newest + 1) {
        throws->count = 0;
    }
    throws->newest = throws->now.number;
    throws->count = throws->count < CHARGE_THROWS_KEPT ? throws->count + 1 : CHARGE_THROWS_KEPT;
    rtos_critical_exit();
}


uint16_t charge_throws_newest(void) {
    return throws != NULL ? throws->newest : 0;
}


uint16_t charge_throws_first(void) {
    if (throws == NULL || throws->count == 0) {
        return 0;
    }
    return (uint16_t) (throws->newest - throws->count + 1);
}


bool charge_throws_get(uint16_t number, charge_throw_t *record) {
    if (throws == NULL) {
        return false;
    }
    rtos_critical_enter();
    uint16_t newest = throws->newest;
    bool kept = throws->count > 0 && number >= 1 && number <= newest && newest - number < throws->count;
    if (kept) {
        *record = throws->kept[number % CHARGE_THROWS_KEPT];
    }
    rtos_critical_exit();
    return kept;
}


static void write_member(char *out, size_t room, size_t *len, const char *name) {
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
}


static void write_float_member(char *out, size_t room, size_t *len, const char *name, float value) {
    write_member(out, room, len, name);
    http_json_float(out, room, len, value);
}


// A tube's stop: its reading and its speed, or null when the throw did not reach it
static void write_stop(char *out, size_t room, size_t *len, const char *name, bool reached, float weight, float rps) {
    write_member(out, room, len, name);
    if (!reached) {
        http_json_raw(out, room, len, "null");
        return;
    }
    http_json_raw(out, room, len, "{\"weight\":");
    http_json_float(out, room, len, weight);
    write_float_member(out, room, len, "rps", rps);
    http_json_raw(out, room, len, "}");
}


// A switch it was thrown on, or null when it never reached its pour
static void write_switch(char *out, size_t room, size_t *len, const char *name, bool known, bool on) {
    write_member(out, room, len, name);
    if (known) {
        http_json_bool(out, room, len, on);
    }
    else {
        http_json_raw(out, room, len, "null");
    }
}


void charge_throws_write_json(const charge_throw_t *record, char *out, size_t room, size_t *len) {
    http_json_raw(out, room, len, "{\"throw\":");
    http_json_integer(out, room, len, (long) record->number);
    write_member(out, room, len, "at_ms");
    // A 64-bit clock, past 32 bits after 49 days: its digits written here, not by the SDK's printf (as events.c)
    char digits[24];
    size_t pos = sizeof(digits) - 1;
    digits[pos] = '\0';
    uint64_t at_ms = record->at_ms;
    do {
        digits[--pos] = (char) ('0' + at_ms % 10);
        at_ms /= 10;
    } while (at_ms > 0 && pos > 0);
    http_json_raw(out, room, len, digits + pos);
    write_member(out, room, len, "simulated");
    http_json_bool(out, room, len, record->simulated);
    write_member(out, room, len, "charge_profile");
    http_json_integer(out, room, len, record->charge_profile);
    write_member(out, room, len, "powder_profile");
    http_json_integer(out, room, len, record->powder_profile);
    write_float_member(out, room, len, "target", record->target);
    write_member(out, room, len, "result");
    http_json_string(out, room, len, events_result_name((event_result_t) record->result));
    write_member(out, room, len, "reason");
    http_json_string(out, room, len, events_reason_name((event_reason_t) record->reason));
    write_float_member(out, room, len, "weight", record->weight);
    write_float_member(out, room, len, "elapsed_s", record->elapsed_s);
    write_float_member(out, room, len, "coarse_s", record->coarse_s);
    write_float_member(out, room, len, "fine_s", record->fine_s);
    // A stop reached has its speed, always a number; its reading may not be (none had come)
    write_stop(out, room, len, "coarse_stop", isfinite(record->coarse_stop_rps), record->coarse_stop_weight,
               record->coarse_stop_rps);
    write_stop(out, room, len, "fine_stop", isfinite(record->fine_stop_rps), record->fine_stop_weight,
               record->fine_stop_rps);
    // What landed after the fine stop: only for a throw given its verdict
    bool verdict = record->result != (uint8_t) EVENT_RESULT_NONE;
    write_float_member(out, room, len, "fall", verdict ? record->weight - record->fine_stop_weight : NAN);
    write_member(out, room, len, "pauses");
    http_json_integer(out, room, len, record->pauses);
    write_member(out, room, len, "gains");
    if (record->configured) {
        http_json_raw(out, room, len, "{\"coarse\":{\"kp\":");
        http_json_float(out, room, len, record->coarse_kp);
        write_float_member(out, room, len, "ki", record->coarse_ki);
        write_float_member(out, room, len, "kd", record->coarse_kd);
        http_json_raw(out, room, len, "},\"fine\":{\"kp\":");
        http_json_float(out, room, len, record->fine_kp);
        write_float_member(out, room, len, "ki", record->fine_ki);
        write_float_member(out, room, len, "kd", record->fine_kd);
        http_json_raw(out, room, len, "},\"by\":");
        http_json_string(out, room, len, record->by_tuner ? "auto_tune" : "profile");
        http_json_raw(out, room, len, "}");
    }
    else {
        http_json_raw(out, room, len, "null");
    }
    write_float_member(out, room, len, "coarse_threshold", record->coarse_threshold);
    write_float_member(out, room, len, "fine_threshold", record->fine_threshold);
    write_switch(out, room, len, "forecast", record->configured, record->forecast);
    write_switch(out, room, len, "auto_tune", record->configured, record->auto_tune);
    write_switch(out, room, len, "backoff", record->configured, record->backoff);
    http_json_raw(out, room, len, "}");
}
