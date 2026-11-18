/*
 * API v1's commands and the unit's reads (ADR-003, gate 4; api_v1.md): the charge, the cleanup, the stop of either,
 * the event log, the scale and its zero, the gate, the motors, the system with its restart and factory reset, the network.
 * Each command is one entry point to the application, which the console and the menu share, and none waits for
 * another task: a start is a request the menu takes, a stop a request the mode takes (activity.h).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "hardware/watchdog.h"
#include "pico/time.h"

#include "activity.h"
#include "api_v1.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "eeprom.h"
#include "events.h"
#include "http_json.h"
#include "http_server.h"
#include "menu.h"
#include "motors.h"
#include "profile.h"
#include "scale.h"
#include "servo_gate.h"
#include "settings.h"
#include "update.h"
#include "update_poll.h"
#include "version.h"
#include "wireless.h"

extern charge_mode_config_t charge_mode_config;
extern scale_config_t scale_config;
extern servo_gate_t servo_gate;

#define API_V1_EVENTS_WAIT_S_MAX 30
#define API_V1_CLEANUP_TIME_S_MIN 1.0f
#define API_V1_CLEANUP_TIME_S_MAX 600.0f
// Over the API a zero while the unit waits for zero puts right only a drift: within this many times the zero test's
// margin (api_v1.md, "A zero over the API")
#define API_V1_ZERO_BAND_MARGINS 5.0f
#define API_V1_RESTART_DELAY_MS 500     // the reply is out before the unit restarts


// --- Requests and errors

// The body as a flat object; false with the error sent
static bool api_body(const http_server_request_t *request, http_server_reply_t *reply, http_json_member_t *members,
                     size_t *count) {
    const char *why;
    if (!http_json_parse_object(request->body, request->body_len, members, HTTP_JSON_MEMBERS_MAX, count, &why)) {
        http_server_reply_error(reply, 400, "bad_json", why);
        return false;
    }
    return true;
}


static const http_json_member_t *api_member(const http_json_member_t *members, size_t count, const char *name) {
    for (size_t idx = 0; idx < count; idx += 1) {
        if (http_json_name_is(&members[idx], name)) {
            return &members[idx];
        }
    }
    return NULL;
}


// An error with what it carries beside its code and message: `extra` is JSON members, or NULL
static void api_error(http_server_reply_t *reply, int status, const char *code, const char *field,
                      const char *message, const char *extra) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"error\":{\"code\":");
    http_json_string(out, room, &len, code);
    if (field != NULL) {
        http_json_raw(out, room, &len, ",\"field\":");
        http_json_string(out, room, &len, field);
    }
    http_json_raw(out, room, &len, ",\"message\":");
    http_json_string(out, room, &len, message);
    if (extra != NULL) {
        http_json_raw(out, room, &len, ",");
        http_json_raw(out, room, &len, extra);
    }
    http_json_raw(out, room, &len, "}}");
    http_server_reply_send(reply, status, "application/json", len);
}


// Fields other than these refused (api_v1.md: unknown fields are refused); false with the error sent
static bool api_only(http_server_reply_t *reply, const http_json_member_t *members, size_t count,
                     const char *const *allowed, size_t allowed_count) {
    for (size_t idx = 0; idx < count; idx += 1) {
        bool known = false;
        for (size_t name = 0; name < allowed_count; name += 1) {
            known = known || http_json_name_is(&members[idx], allowed[name]);
        }
        if (!known) {
            char field[40];
            size_t len = members[idx].name_len < sizeof(field) ? members[idx].name_len : sizeof(field) - 1;
            memcpy(field, members[idx].name, len);
            field[len] = '\0';
            api_error(reply, 400, "unknown_field", field, "no such field here", NULL);
            return false;
        }
    }
    return true;
}


static const char *update_state_name(update_state_t state);


const char *api_v1_job_state(void) {
    activity_t activity = activity_get();
    if (activity == ACTIVITY_CHARGE) {
        charge_status_t charge;
        charge_mode_get_status(&charge);
        return charge.state;
    }
    if (activity == ACTIVITY_CLEANUP) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        return cleanup.state;
    }
    if (activity == ACTIVITY_UPDATE) {
        update_status_t update;
        update_status(&update);
        return update_state_name(update.state);
    }
    return "idle";
}


// 409 busy, with the activity and the job's state
static void api_busy(http_server_reply_t *reply, const char *message) {
    char extra[96];
    snprintf(extra, sizeof(extra), "\"activity\":\"%s\",\"state\":\"%s\"", activity_name(activity_get()),
             api_v1_job_state());
    api_error(reply, 409, "busy", NULL, message, extra);
}


static void api_driver_fault(http_server_reply_t *reply) {
    char extra[64];
    const char *coarse = motor_driver_get_status_string(SELECT_COARSE_TRICKLER_MOTOR);
    bool coarse_faulted = coarse != NULL && strcmp(coarse, "ok") != 0;
    snprintf(extra, sizeof(extra), "\"motor\":\"%s\",\"fault\":\"%s\"", coarse_faulted ? "coarse" : "fine",
             coarse_faulted ? coarse : motor_driver_get_status_string(SELECT_FINE_TRICKLER_MOTOR));
    const char *fault = motors_get_fault();
    api_error(reply, 409, "driver_fault", NULL, fault != NULL ? fault : "a motor driver is not OK", extra);
}


// The body is {}: false with the error sent
static bool api_empty(const http_server_request_t *request, http_server_reply_t *reply) {
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    return api_body(request, reply, members, &count) && api_only(reply, members, count, NULL, 0);
}


static void api_send(http_server_reply_t *reply, int status, size_t room, size_t len) {
    if (!http_json_fits(room, len)) {
        http_server_reply_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    http_server_reply_send(reply, status, "application/json", len);
}


static void write_u64(char *out, size_t room, size_t *len, uint64_t value) {
    char digits[24];
    size_t pos = sizeof(digits) - 1;
    digits[pos] = '\0';
    do {
        digits[--pos] = (char) ('0' + value % 10);
        value /= 10;
    } while (value > 0 && pos > 0);
    http_json_raw(out, room, len, digits + pos);
}


// --- The charge

static void write_charge(char *out, size_t room, size_t *len) {
    charge_status_t charge;
    charge_mode_get_status(&charge);
    uint16_t profile = profile_get_selected_idx();
    http_json_raw(out, room, len, "{\"state\":");
    http_json_string(out, room, len, charge.state);
    http_json_raw(out, room, len, ",\"in_throw\":");
    http_json_bool(out, room, len, charge.in_throw);
    http_json_raw(out, room, len, ",\"throw\":");
    http_json_integer(out, room, len, charge.throw_number);
    http_json_raw(out, room, len, ",\"paused\":");
    http_json_bool(out, room, len, charge.paused);
    http_json_raw(out, room, len, ",\"target\":");
    if (charge.target > 0.0f) {
        http_json_float(out, room, len, charge.target);
    }
    else {
        http_json_raw(out, room, len, "null");
    }
    http_json_raw(out, room, len, ",\"powder_profile\":");
    http_json_integer(out, room, len, profile);
    http_json_raw(out, room, len, ",\"powder_profile_name\":");
    http_json_string(out, room, len, profile_get_name(profile));
    http_json_raw(out, room, len, ",\"weight\":");
    http_json_number(out, room, len, scale_get_current_measurement(), 6);
    http_json_raw(out, room, len, ",\"elapsed_s\":");
    http_json_number(out, room, len, charge.elapsed_s, 4);
    http_json_raw(out, room, len, ",\"last\":{\"throw\":");
    http_json_integer(out, room, len, charge.last_throw);
    http_json_raw(out, room, len, ",\"result\":");
    http_json_string(out, room, len, events_result_name(charge.last_result));
    http_json_raw(out, room, len, ",\"reason\":");
    http_json_string(out, room, len, events_reason_name(charge.last_reason));
    http_json_raw(out, room, len, ",\"weight\":");
    http_json_number(out, room, len, charge.last_weight, 6);
    http_json_raw(out, room, len, "},\"event_seq\":");
    http_json_integer(out, room, len, (long) events_latest_seq());
    http_json_raw(out, room, len, "}");
}


static void reply_charge(http_server_reply_t *reply, int status) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    write_charge(out, room, &len);
    api_send(reply, status, room, len);
}


static void api_v1_get_charge(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_charge(reply, 200);
}


/*
 * `target` and `powder_profile`, as a PATCH and a start carry them, checked; nothing written. False with the error
 * sent
 */
static bool charge_fields(http_server_reply_t *reply, const http_json_member_t *members, size_t count,
                          bool *has_target, float *target, bool *has_profile, uint16_t *profile) {
    static const char *const allowed[] = {"target", "powder_profile"};
    if (!api_only(reply, members, count, allowed, 2)) {
        return false;
    }
    const http_json_member_t *target_member = api_member(members, count, "target");
    const http_json_member_t *profile_member = api_member(members, count, "powder_profile");
    *has_target = target_member != NULL;
    *has_profile = profile_member != NULL;
    if (*has_target) {
        double value;
        if (!http_json_number_value(target_member, &value)) {
            api_error(reply, 400, "wrong_type", "target", "target is a number", NULL);
            return false;
        }
        *target = (float) value;
        if (!charge_mode_target_fits(*target)) {
            api_error(reply, 400, "out_of_range", "target",
                      charge_mode_config.eeprom_charge_mode_data.decimal_places == DP_3 ?
                      "a target the menu can show: 0.001 to 99.999, at three decimal places" :
                      "a target the menu can show: 0.01 to 999.99, at two decimal places", NULL);
            return false;
        }
    }
    if (*has_profile) {
        double value;
        if (!http_json_number_value(profile_member, &value) || value != floor(value)) {
            api_error(reply, 400, "wrong_type", "powder_profile", "powder_profile is a profile's id", NULL);
            return false;
        }
        if (value < 0 || value >= MAX_PROFILE_CNT) {
            api_error(reply, 400, "out_of_range", "powder_profile", "no powder profile has that id", NULL);
            return false;
        }
        *profile = (uint16_t) value;
    }
    return true;
}


// PATCH /api/v1/charge: what the next throw uses; refused while a throw is under way
static void api_v1_patch_charge(const http_server_request_t *request, http_server_reply_t *reply) {
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    bool has_target;
    bool has_profile;
    float target = 0.0f;
    uint16_t profile = 0;
    if (!api_body(request, reply, members, &count) ||
        !charge_fields(reply, members, count, &has_target, &target, &has_profile, &profile)) {
        return;
    }
    activity_lock();
    bool in_throw = activity_in_throw();
    if (!in_throw) {
        if (has_target) {
            charge_mode_set_target(target);
        }
        if (has_profile) {
            profile_select((uint8_t) profile);
        }
    }
    activity_unlock();
    if (in_throw) {
        api_busy(reply, "the charge's target and powder profile are refused while a throw is under way");
        return;
    }
    reply_charge(reply, 200);
}


// POST /api/v1/charge/start: {}, or the target and the powder profile to set first. Refused, it changes nothing
static void api_v1_charge_start(const http_server_request_t *request, http_server_reply_t *reply) {
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    bool has_target;
    bool has_profile;
    float target = 0.0f;
    uint16_t profile = 0;
    if (!api_body(request, reply, members, &count) ||
        !charge_fields(reply, members, count, &has_target, &target, &has_profile, &profile)) {
        return;
    }
    switch (charge_mode_request_start(ACTIVITY_BY_API, has_target ? target : NAN, has_profile ? (int) profile : -1)) {
        case CHARGE_START_OK:
            reply_charge(reply, 202);
            return;
        case CHARGE_START_DRIVER_FAULT:
            api_driver_fault(reply);
            return;
        case CHARGE_START_NO_TARGET:
            api_error(reply, 400, "no_target", "target", "no target is set: give one", NULL);
            return;
        default:
            api_busy(reply, "another job runs, a start is asked already, or a person has a screen open at the unit");
            return;
    }
}


// POST /api/v1/charge/stop: never fails; it acknowledges `fault` too
static void api_v1_charge_stop(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    reply_charge(reply, activity_request_stop(ACTIVITY_CHARGE, ACTIVITY_BY_API) ? 202 : 200);
}


// --- The purge

static void reply_purge(http_server_reply_t *reply, int status) {
    cleanup_status_t cleanup;
    cleanup_mode_get_status(&cleanup);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"state\":");
    http_json_string(out, room, &len, cleanup.state);
    http_json_raw(out, room, &len, ",\"by\":");
    if (cleanup.by != NULL) {
        http_json_string(out, room, &len, cleanup.by);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, ",\"speed_rps\":");
    http_json_float(out, room, &len, cleanup.speed_rps);
    http_json_raw(out, room, &len, ",\"time_left_s\":");
    http_json_number(out, room, &len, cleanup.time_left_s, 4);
    http_json_raw(out, room, &len, "}");
    api_send(reply, status, room, len);
}


static void api_v1_get_purge(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_purge(reply, 200);
}


// PATCH /api/v1/purge: {"speed_rps": 2.0}, or with "time_s", at most that long
static void api_v1_patch_purge(const http_server_request_t *request, http_server_reply_t *reply) {
    static const char *const allowed[] = {"speed_rps", "time_s"};
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 2)) {
        return;
    }
    const http_json_member_t *speed_member = api_member(members, count, "speed_rps");
    const http_json_member_t *time_member = api_member(members, count, "time_s");
    double speed;
    if (speed_member == NULL || !http_json_number_value(speed_member, &speed)) {
        api_error(reply, 400, "wrong_type", "speed_rps", "speed_rps is a number, and is given", NULL);
        return;
    }
    float time_s = NAN;
    if (time_member != NULL) {
        double value;
        if (!http_json_number_value(time_member, &value)) {
            api_error(reply, 400, "wrong_type", "time_s", "time_s is a number", NULL);
            return;
        }
        if (value < API_V1_CLEANUP_TIME_S_MIN || value > API_V1_CLEANUP_TIME_S_MAX) {
            api_error(reply, 400, "out_of_range", "time_s", "time_s is 1 to 600", "\"min\":1,\"max\":600");
            return;
        }
        time_s = (float) value;
    }
    if (!cleanup_mode_set_speed((float) speed, time_s)) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        api_error(reply, 409, "not_running", NULL, strcmp(cleanup.state, "fault") == 0 ?
                  "the purge ended on a driver's fault: stop it to acknowledge, then start another" :
                  "no purge runs: start one first", NULL);
        return;
    }
    reply_purge(reply, 200);
}


static void api_v1_purge_start(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    switch (cleanup_mode_request_start(ACTIVITY_BY_API)) {
        case CLEANUP_START_OK:
            reply_purge(reply, 202);
            return;
        case CLEANUP_START_DRIVER_FAULT:
            api_driver_fault(reply);
            return;
        default:
            api_busy(reply, "another job runs, a start is asked already, or a person has a screen open at the unit");
            return;
    }
}


static void api_v1_purge_stop(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    reply_purge(reply, activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_API) ? 202 : 200);
}


// POST /api/v1/stop: whatever job runs; never fails
static void api_v1_stop(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    activity_t before = activity_get();
    bool stopped = activity_request_stop(ACTIVITY_NONE, ACTIVITY_BY_API);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"stopped\":");
    http_json_bool(out, room, &len, stopped);
    http_json_raw(out, room, &len, ",\"activity\":");
    http_json_string(out, room, &len, activity_name(before));
    http_json_raw(out, room, &len, "}");
    api_send(reply, stopped ? 202 : 200, room, len);
}


// --- The event log

typedef struct {
    uint32_t after;     // asked after this number
    uint32_t cursor;    // the next to write
    uint32_t last;      // the newest when the reply began
    uint8_t phase;      // 0 the head, 1 the events, 2 the end
    bool wrote_one;     // an event is out: the next takes a comma
} api_v1_events_t;


static bool api_v1_events_more(void *ctx, char *buf, size_t room, size_t *len) {
    api_v1_events_t *events = ctx;
    *len = 0;
    if (events->phase == 0) {
        uint32_t first = events_first_seq();
        http_json_raw(buf, room, len, "{\"boot\":");
        http_json_unsigned(buf, room, len, api_v1_boot_id());
        http_json_raw(buf, room, len, ",\"now_ms\":");
        write_u64(buf, room, len, time_us_64() / 1000u);
        http_json_raw(buf, room, len, ",\"first\":");
        http_json_integer(buf, room, len, (long) first);
        http_json_raw(buf, room, len, ",\"next\":");
        http_json_integer(buf, room, len, (long) events->last);
        http_json_raw(buf, room, len, ",\"events\":[");
        events->cursor = events->after + 1 > first ? events->after + 1 : first;
        events->phase = 1;
        return true;
    }
    if (events->phase == 1) {
        while (events->cursor <= events->last) {
            event_t event;
            if (!events_get(events->cursor, &event)) {
                events->cursor += 1;    // gone while the reply was made: the client sees the gap
                continue;
            }
            size_t mark = *len;
            if (events->wrote_one) {
                http_json_raw(buf, room, len, ",");
            }
            events_write_json(&event, buf, room, len);
            if (!http_json_fits(room, *len)) {
                *len = mark;    // the next chunk begins with it
                buf[mark] = '\0';
                return true;
            }
            events->wrote_one = true;
            events->cursor += 1;
        }
        events->phase = 2;
        return true;
    }
    http_json_raw(buf, room, len, "]}");
    return false;
}


static void api_v1_events_reply(http_server_reply_t *reply, uint32_t after) {
    api_v1_events_t *events = http_server_reply_context(reply, sizeof(api_v1_events_t));
    memset(events, 0, sizeof(*events));
    events->after = after;
    events->last = events_latest_seq();
    http_server_reply_stream(reply, 200, "application/json", api_v1_events_more, events);
}


static bool api_v1_events_ready(void *ctx) {
    return events_latest_seq() > *(uint32_t *) ctx;
}


static void api_v1_events_resume(const http_server_request_t *request, http_server_reply_t *reply) {
    char value[16];
    uint32_t after = 0;
    if (http_server_query_value(request->query, "after", value, sizeof(value))) {
        after = (uint32_t) strtoul(value, NULL, 10);
    }
    api_v1_events_reply(reply, after);
}


// GET /api/v1/events?after=N[&wait=s]: the events after N; with wait, held until there is one or the time is out
static void api_v1_get_events(const http_server_request_t *request, http_server_reply_t *reply) {
    char value[16];
    uint32_t after = 0;
    if (http_server_query_value(request->query, "after", value, sizeof(value))) {
        char *end;
        unsigned long parsed = strtoul(value, &end, 10);
        if (end == value || *end != '\0') {
            api_error(reply, 400, "wrong_type", "after", "after is an event's number", NULL);
            return;
        }
        after = (uint32_t) parsed;
    }
    if (http_server_query_value(request->query, "wait", value, sizeof(value))) {
        long wait_s = strtol(value, NULL, 10);
        if (wait_s < 1 || wait_s > API_V1_EVENTS_WAIT_S_MAX) {
            api_error(reply, 400, "out_of_range", "wait", "wait is 1 to 30 s", "\"min\":1,\"max\":30");
            return;
        }
        // Held only when the client is up to date. After a number above the newest it read before a restart: answered
        // at once, so it sees the new boot now, not when its hold ends (review Q2)
        if (events_latest_seq() == after) {
            uint32_t *held_after = http_server_reply_context(reply, sizeof(uint32_t));
            *held_after = after;
            http_server_reply_park(reply, api_v1_events_ready, held_after, (uint32_t) wait_s * 1000,
                                   api_v1_events_resume);
            return;
        }
    }
    api_v1_events_reply(reply, after);
}


// --- The scale

static void reply_scale(http_server_reply_t *reply, int status) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    const char *unit = scale_get_unit();
    http_json_raw(out, room, &len, "{\"weight\":");
    http_json_number(out, room, &len, scale_get_current_measurement(), 6);
    http_json_raw(out, room, &len, ",\"unit\":");
    if (unit != NULL) {
        http_json_string(out, room, &len, unit);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, ",\"driver\":");
    http_json_string(out, room, &len, scale_driver_api_name(scale_get_running_driver()));
    http_json_raw(out, room, &len, ",\"simulated\":");
    http_json_bool(out, room, &len, scale_simulation_is_on());
    http_json_raw(out, room, &len, ",\"usual_interval_ms\":");
    http_json_integer(out, room, &len, (long) scale_get_usual_interval_ms());
    http_json_raw(out, room, &len, ",\"bad_frames\":");
    http_json_integer(out, room, &len, (long) scale_get_bad_frames());
    http_json_raw(out, room, &len, scale_can_zero() ? ",\"actions\":[\"zero\"]}" : ",\"actions\":[]}");
    api_send(reply, status, room, len);
}


static void api_v1_get_scale(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_scale(reply, 200);
}


/*
 * POST /api/v1/scale/zero (api_v1.md, "A zero over the API"): taken with no job, in a cleanup and in `fault`; while
 * the unit waits for zero only to put right a drift; refused once a throw pours. Checked and sent under the lock the
 * charge takes to pour, so no zero lands as a throw begins to pour.
 */
static void api_v1_scale_zero(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    if (!scale_can_zero()) {
        api_error(reply, 501, "not_supported", NULL, "this make of scale cannot be asked to zero", NULL);
        return;
    }
    activity_lock();
    activity_t activity = activity_get();
    charge_status_t charge;
    charge_mode_get_status(&charge);
    float weight = scale_get_current_measurement();
    float band = API_V1_ZERO_BAND_MARGINS * charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin;
    const char *refused = NULL;
    bool off_zero = false;
    // A charge asked and not yet begun (`starting`) is before its pour, as `zeroing` is: its first throw pours from
    // this zero, so only a drift is zeroed. Before, a zero then was taken whatever the pan held (review D3)
    bool before_pour = strcmp(charge.state, "starting") == 0 || strcmp(charge.state, "zeroing") == 0;
    if (before_pour) {
        off_zero = !(fabsf(weight) <= band);
    }
    else if (activity == ACTIVITY_CHARGE && strcmp(charge.state, "fault") != 0) {
        refused = "a throw pours, or the unit waits for the cup: a zero now would start a throw on whatever is on "
                  "the pan";
    }
    if (refused == NULL && !off_zero) {
        scale_ask_zero();
    }
    activity_unlock();

    if (refused != NULL) {
        api_busy(reply, refused);
        return;
    }
    if (off_zero) {
        char extra[48];
        char message[160];
        size_t used = 0;
        char weight_text[24];
        weight_text[0] = '\0';
        http_json_number(weight_text, sizeof(weight_text), &used, weight, 6);
        snprintf(extra, sizeof(extra), "\"weight\":%s", weight_text);
        snprintf(message, sizeof(message), "the scale reads %s; over the API a zero puts right only a drift; empty "
                 "the cup or put it on, or zero at the unit", weight_text);
        api_error(reply, 409, "weight_off_zero", NULL, message, extra);
        return;
    }
    reply_scale(reply, 200);
}


// --- The gate

static void reply_gate(http_server_reply_t *reply, int status) {
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    const char *state;
    bool has_ratio = true;
    float ratio = servo_gate.gate_ratio;
    if (!servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        state = "disabled";
        has_ratio = false;
    }
    else if (servo_gate.gate_state == GATE_DISABLED) {
        state = "held";     // the menu's "Disable": the servo moved no more, at its last duty
    }
    else if (ratio <= 0.0001f) {
        state = "open";
    }
    else if (ratio >= 0.9999f) {
        state = "closed";
    }
    else {
        state = "partial";
    }
    http_json_raw(out, room, &len, "{\"state\":");
    http_json_string(out, room, &len, state);
    http_json_raw(out, room, &len, ",\"ratio\":");
    if (has_ratio) {
        http_json_float(out, room, &len, ratio);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, "}");
    api_send(reply, status, room, len);
}


static void api_v1_get_gate(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_gate(reply, 200);
}


// PATCH /api/v1/gate: {"ratio": 0.5}, 0 open to 1 closed; the move is the gate's own task's, not waited for
static void api_v1_patch_gate(const http_server_request_t *request, http_server_reply_t *reply) {
    static const char *const allowed[] = {"ratio"};
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 1)) {
        return;
    }
    const http_json_member_t *ratio_member = api_member(members, count, "ratio");
    double ratio;
    if (ratio_member == NULL || !http_json_number_value(ratio_member, &ratio)) {
        api_error(reply, 400, "wrong_type", "ratio", "ratio is a number, and is given", NULL);
        return;
    }
    if (ratio < 0.0 || ratio > 1.0) {
        api_error(reply, 400, "out_of_range", "ratio", "ratio is 0, open, to 1, closed", "\"min\":0,\"max\":1");
        return;
    }
    if (!servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        api_error(reply, 409, "gate_disabled", NULL, "the gate is off in its settings", NULL);
        return;
    }
    activity_lock();
    bool in_throw = activity_in_throw();
    if (!in_throw) {
        servo_gate_set_ratio((gate_ratio_t) ratio, false);
    }
    activity_unlock();
    if (in_throw) {
        api_busy(reply, "the gate is the throw's while one is under way");
        return;
    }
    reply_gate(reply, 200);
}


// --- The motors

static void write_motor(char *out, size_t room, size_t *len, motor_select_t motor) {
    http_json_raw(out, room, len, "{\"speed_rps\":");
    http_json_float(out, room, len, motor_get_velocity(motor));
    http_json_raw(out, room, len, ",\"driver\":");
    const char *driver = motor_driver_get_status_string(motor);
    http_json_string(out, room, len, driver != NULL ? driver : "absent");
    http_json_raw(out, room, len, "}");
}


static void api_v1_get_motors(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"coarse\":");
    write_motor(out, room, &len, SELECT_COARSE_TRICKLER_MOTOR);
    http_json_raw(out, room, &len, ",\"fine\":");
    write_motor(out, room, &len, SELECT_FINE_TRICKLER_MOTOR);
    http_json_raw(out, room, &len, "}");
    api_send(reply, 200, room, len);
}


// --- The update (ADR-004; api_v1.md, "Firmware updates")

static const char *update_state_name(update_state_t state) {
    switch (state) {
        case UPDATE_STARTING: return "starting";
        case UPDATE_RECEIVING: return "receiving";
        case UPDATE_READY: return "ready";
        case UPDATE_INSTALLING: return "installing";
        case UPDATE_ENDING: return "ending";
        case UPDATE_IDLE:
        default: return "idle";
    }
}


static const char *update_result_name(update_result_t result) {
    switch (result) {
        case UPDATE_RESULT_CONFIRMED: return "confirmed";
        case UPDATE_RESULT_BACK: return "back";
        case UPDATE_RESULT_REFUSED: return "refused";
        case UPDATE_RESULT_STOPPED: return "stopped";
        case UPDATE_RESULT_TIME: return "time";
        case UPDATE_RESULT_NONE:
        default: return "none";
    }
}


static void write_text_or_null(char *out, size_t room, size_t *len, const char *text) {
    if (text != NULL) {
        http_json_string(out, room, len, text);
    }
    else {
        http_json_raw(out, room, len, "null");
    }
}


static void reply_update(http_server_reply_t *reply, int status) {
    update_status_t update;
    update_status(&update);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    char slot[2] = {update.slot, '\0'};
    http_json_raw(out, room, &len, "{\"state\":");
    http_json_string(out, room, &len, update_state_name(update.state));
    http_json_raw(out, room, &len, ",\"running\":{\"version\":");
    http_json_string(out, room, &len, update.version);
    http_json_raw(out, room, &len, ",\"block\":");
    http_json_string(out, room, &len, update.block);
    http_json_raw(out, room, &len, ",\"slot\":");
    http_json_string(out, room, &len, slot);
    http_json_raw(out, room, &len, ",\"trial\":");
    http_json_bool(out, room, &len, update.on_trial);
    http_json_raw(out, room, &len, "},\"takes\":");
    http_json_string(out, room, &len, update.takes);
    http_json_raw(out, room, &len, ",\"free_bytes\":");
    http_json_integer(out, room, &len, (long) update.free_bytes);
    http_json_raw(out, room, &len, ",\"received\":");
    http_json_integer(out, room, &len, (long) update.received);
    http_json_raw(out, room, &len, ",\"expected\":");
    http_json_integer(out, room, &len, (long) update.expected);
    http_json_raw(out, room, &len, ",\"download\":");
    http_json_bool(out, room, &len, update.download);
    // The poll (ADR-010, gate 5): its state, and the release it found
    update_poll_status_t poll;
    update_poll_status(&poll);
    static const char *const poll_states[] = {"idle", "checking", "up_to_date", "found", "failed"};
    http_json_raw(out, room, &len, ",\"poll\":{\"state\":");
    http_json_string(out, room, &len, poll_states[poll.state]);
    http_json_raw(out, room, &len, ",\"error\":");
    write_text_or_null(out, room, &len, poll.state == UPDATE_POLL_FAILED ? poll.error : NULL);
    http_json_raw(out, room, &len, "},\"found\":");
    if (poll.state != UPDATE_POLL_FOUND) {
        http_json_raw(out, room, &len, "null");
    }
    else {
        char block[16];
        update_version_text(poll.block, block, sizeof(block));
        http_json_raw(out, room, &len, "{\"version\":");
        http_json_string(out, room, &len, poll.version);
        http_json_raw(out, room, &len, ",\"block\":");
        http_json_string(out, room, &len, block);
        http_json_raw(out, room, &len, ",\"note\":");
        http_json_string(out, room, &len, poll.note);
        http_json_raw(out, room, &len, ",\"importance\":");
        http_json_string(out, room, &len, poll.critical ? "critical" : "normal");
        http_json_raw(out, room, &len, ",\"channel\":");
        http_json_string(out, room, &len, poll.channel == UPDATE_CHANNEL_BETA ? "beta" : "stable");
        // Where it was found: the bench host's address, or null from the stored source (ADR-010, gate 6, step 9)
        http_json_raw(out, room, &len, ",\"bench\":");
        write_text_or_null(out, room, &len, poll.bench[0] != '\0' ? poll.bench : NULL);
        http_json_raw(out, room, &len, "}");
    }
    http_json_raw(out, room, &len, ",\"last\":");
    if (update.last.result == UPDATE_RESULT_NONE) {
        http_json_raw(out, room, &len, "null");
    }
    else {
        http_json_raw(out, room, &len, "{\"result\":");
        http_json_string(out, room, &len, update_result_name(update.last.result));
        char version[16];
        if (update.last.version != 0) {
            update_version_text(update.last.version, version, sizeof(version));
        }
        http_json_raw(out, room, &len, ",\"version\":");
        write_text_or_null(out, room, &len, update.last.version != 0 ? version : NULL);
        http_json_raw(out, room, &len, ",\"code\":");
        write_text_or_null(out, room, &len, update.last.code);
        http_json_raw(out, room, &len, ",\"message\":");
        write_text_or_null(out, room, &len, update.last.message);
        http_json_raw(out, room, &len, "}");
    }
    http_json_raw(out, room, &len, "}");
    api_send(reply, status, room, len);
}


static void api_v1_get_update(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_update(reply, 200);
}


static void api_v1_update_start(const http_server_request_t *request, http_server_reply_t *reply) {
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    static const char *const allowed[] = {"download"};
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 1)) {
        return;
    }
    // "download": true, the job fetches the release the poll found itself, and installs it once checked (ADR-010)
    const http_json_member_t *download = api_member(members, count, "download");
    if (download != NULL && download->kind != HTTP_JSON_BOOLEAN) {
        api_error(reply, 400, "wrong_type", "download", "true or false", NULL);
        return;
    }
    bool fetch = download != NULL && download->value[0] == 't';
    switch (fetch ? update_request_download(ACTIVITY_BY_API) : update_request_start(ACTIVITY_BY_API)) {
        case UPDATE_START_OK:
            reply_update(reply, 202);
            break;
        case UPDATE_START_NOTHING_FOUND:
            api_error(reply, 409, "not_found", NULL, "no update is found: POST /api/v1/update/check first", NULL);
            break;
        case UPDATE_START_NO_SLOTS:
            api_error(reply, 500, "failed", NULL, "the image slots are not known: no update can be written", NULL);
            break;
        case UPDATE_START_ON_TRIAL:
            api_error(reply, 409, "busy", NULL,
                      "the image running is on trial: an update waits until it is confirmed on its network", NULL);
            break;
        case UPDATE_START_BUSY:
        default:
            api_busy(reply, "a job runs, a start is asked, a poll runs, or a person has a view open at the unit");
            break;
    }
}


// POST /api/v1/update/check: a poll of the update's source now (ADR-010, gate 5)
static void api_v1_update_check(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    switch (update_poll_ask()) {
        case UPDATE_POLL_ASKED:
            reply_update(reply, 202);
            break;
        case UPDATE_POLL_NO_NETWORK:
            api_error(reply, 409, "no_network", NULL,
                      "not joined to a network as a station: the unit's own access point has no internet", NULL);
            break;
        case UPDATE_POLL_BUSY:
        default:
            api_busy(reply, "a poll runs, or the update does");
            break;
    }
}


static void api_v1_update_install(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    if (!update_install(ACTIVITY_BY_API)) {
        api_error(reply, 409, "not_ready", NULL, "no image is ready: PUT /api/v1/update/image first", NULL);
        return;
    }
    reply_update(reply, 202);
}


static void api_v1_update_stop(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    update_status_t update;
    update_status(&update);
    if (update.state == UPDATE_INSTALLING) {
        api_error(reply, 409, "busy", NULL, "installing: the restart into its trial is under way", NULL);
        return;
    }
    reply_update(reply, activity_request_stop(ACTIVITY_UPDATE, ACTIVITY_BY_API) ? 202 : 200);
}


// PUT /api/v1/update/image: the image in pieces, written into the free slot as it comes (update.c)
static bool api_v1_update_image_begin(const http_server_request_t *request, http_server_reply_t *reply) {
    if (request->body_is_json) {
        http_server_reply_error(reply, 415, "wrong_media_type", "the image is sent as application/octet-stream");
        return false;
    }
    uint32_t *token = http_server_reply_context(reply, sizeof(uint32_t));
    if (token == NULL) {
        api_error(reply, 500, "failed", NULL, "no room for the upload's state", NULL);
        return false;
    }
    int status;
    update_refusal_t refused = update_image_begin((uint32_t) request->body_len, &status, token);
    if (refused.code != NULL) {
        api_error(reply, status, refused.code, NULL, refused.message, NULL);
        return false;
    }
    return true;
}


static bool api_v1_update_image_piece(void *ctx, const char *data, size_t len) {
    return update_image_piece(*(const uint32_t *) ctx, (const uint8_t *) data, len);
}


static void api_v1_update_image_end(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    const uint32_t *token = http_server_reply_context(reply, sizeof(uint32_t));
    int status;
    update_refusal_t refused = update_image_end(token != NULL ? *token : 0, &status);
    if (refused.code != NULL) {
        api_error(reply, status, refused.code, NULL, refused.message, NULL);
        return;
    }
    reply_update(reply, 200);
}


static void api_v1_update_image_abort(void *ctx) {
    update_image_abort(*(const uint32_t *) ctx);
}


static const http_server_upload_t api_v1_update_image = {
    .begin = api_v1_update_image_begin,
    .piece = api_v1_update_image_piece,
    .end = api_v1_update_image_end,
    .abort = api_v1_update_image_abort,
};


// --- The system

static void api_v1_get_system(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    char unit_id[9];
    eeprom_get_board_id(unit_id, 8);
    unit_id[8] = '\0';
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"firmware\":{\"version\":");
    http_json_string(out, room, &len, version_string);
    http_json_raw(out, room, &len, ",\"vcs\":");
    http_json_string(out, room, &len, vcs_hash);
    http_json_raw(out, room, &len, ",\"build\":");
    http_json_string(out, room, &len, build_type);
    http_json_raw(out, room, &len, "},\"board\":");
    http_json_string(out, room, &len, api_v1_board_name());
    http_json_raw(out, room, &len, ",\"unit_id\":");
    http_json_string(out, room, &len, unit_id);
    http_json_raw(out, room, &len, ",\"uptime_s\":");
    http_json_integer(out, room, &len, (long) (time_us_64() / 1000000u));
    http_json_raw(out, room, &len, ",\"activity\":");
    http_json_string(out, room, &len, activity_name(activity_get()));
    http_json_raw(out, room, &len, ",\"restart_pending\":");
    http_json_bool(out, room, &len, settings_restart_pending());
    http_json_raw(out, room, &len, "}");
    api_send(reply, 200, room, len);
}


// A restart, or an erase and a restart, once the reply is out: a task of its own, so the server goes on
static void api_v1_restart_task(void *erase) {
    vTaskDelay(pdMS_TO_TICKS(API_V1_RESTART_DELAY_MS));
    if (erase != NULL) {
        eeprom_erase(true);     // restarts when done
    }
    watchdog_reboot(0, 0, 0);
    vTaskDelete(NULL);
}


static bool api_v1_restart_later(bool erase) {
    return xTaskCreate(api_v1_restart_task, "Restart", 512, erase ? (void *) 1 : NULL, 2, NULL) == pdPASS;
}


// POST /api/v1/agent/hello {"name": "..."}: an agent says it is here, and the screen shows who for about 2 s, flashing
// first after a quiet spell (menu_visit()). The owner's hidden mode, for fun (2026-11-17): not in the index or the
// schema, so only agents that read AGENTS.md know it. It changes nothing but the screen, and never over a charge
static void api_v1_agent_hello(const http_server_request_t *request, http_server_reply_t *reply) {
    static const char *const allowed[] = {"name"};
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 1)) {
        return;
    }
    char name[MENU_VISIT_NAME_MAX + 1] = "Claude-chan";
    const http_json_member_t *member = api_member(members, count, "name");
    if (member != NULL) {
        bool fits = http_json_string_value(member, name, sizeof(name)) && name[0] != '\0';
        for (const char *at = name; fits && *at != '\0'; at += 1) {
            fits = *at >= 0x20 && *at <= 0x7e;      // the screen's fonts have printable ASCII only
        }
        if (!fits) {
            api_error(reply, 400, "bad_value", "name", "printable ASCII, 1 to 16 characters", NULL);
            return;
        }
    }
    bool flash = false;
    if (!menu_visit(name, &flash)) {
        api_busy(reply, "a charge has the screen");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"shown\":true,\"name\":");
    http_json_string(out, room, &len, name);
    http_json_raw(out, room, &len, ",\"flash\":");
    http_json_bool(out, room, &len, flash);
    http_json_raw(out, room, &len, "}");
    api_send(reply, 200, room, len);
}


static void api_v1_system_restart(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    if (activity_job_runs()) {
        api_busy(reply, "a job runs: stop it first");
        return;
    }
    if (!api_v1_restart_later(false)) {
        http_server_reply_error(reply, 500, "failed", "the restart could not be set going");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"restarting\":true}");
    api_send(reply, 202, room, len);
}


static void api_v1_system_factory_reset(const http_server_request_t *request, http_server_reply_t *reply) {
    static const char *const allowed[] = {"confirm"};
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 1)) {
        return;
    }
    const http_json_member_t *confirm = api_member(members, count, "confirm");
    char text[16];
    if (confirm == NULL || confirm->kind != HTTP_JSON_STRING || !http_json_string_value(confirm, text, sizeof(text)) ||
        strcmp(text, "factory_reset") != 0) {
        api_error(reply, 400, "confirm_missing", "confirm", "send {\"confirm\": \"factory_reset\"}: it cannot be undone", NULL);
        return;
    }
    if (activity_job_runs()) {
        api_busy(reply, "a job runs: stop it first");
        return;
    }
    if (!api_v1_restart_later(true)) {
        http_server_reply_error(reply, 500, "failed", "the factory reset could not be set going");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"resetting\":true}");
    api_send(reply, 202, room, len);
}


// --- The network

static void api_v1_get_network(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    wireless_info_t info;
    wireless_get_info(&info);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"mode\":");
    http_json_string(out, room, &len, info.mode);
    http_json_raw(out, room, &len, ",\"ssid\":");
    http_json_string(out, room, &len, info.ssid);
    http_json_raw(out, room, &len, ",\"address\":");
    if (info.address[0]) {
        http_json_string(out, room, &len, info.address);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, ",\"hostname\":");
    http_json_string(out, room, &len, info.hostname);
    http_json_raw(out, room, &len, ",\"rssi\":");
    if (info.has_rssi) {
        http_json_integer(out, room, &len, info.rssi);
    }
    else {
        http_json_raw(out, room, &len, "null");
    }
    http_json_raw(out, room, &len, "}");
    api_send(reply, 200, room, len);
}


/*
    The commands' share of GET /api/v1/schema (gate 4): each action with its body's fields, in the shape of a
    resource's; each kind of state with what each value means; each event kind with what it carries. JSON as sent,
    one item a text, each well under a chunk (settings.c writes them one at a time). `refused_while` is the
    resources' words, and `no_purge_runs` for the cleanup's speed; the zero's rule is in its `about`.
*/
_Static_assert(MAX_PROFILE_CNT == 8, "the schema below gives the powder profiles' ids as 0 to 7");

#define API_V1_SCHEMA_CHARGE_FIELDS \
    "\"fields\":[{\"name\":\"target\",\"type\":\"number\",\"min\":0.001,\"max\":999.99,\"about\":\"in the scale's " \
    "unit; what the menu's digits can show: 0.01 to 999.99 at two decimal places, 0.001 to 99.999 at three " \
    "(settings/charge decimal_places)\"},{\"name\":\"powder_profile\",\"type\":\"integer\",\"min\":0,\"max\":7," \
    "\"about\":\"a powder profile's id (GET /api/v1/powder_profiles)\"}]"

static const char *const api_v1_schema_actions[] = {
    "{\"method\":\"PATCH\",\"path\":\"/api/v1/charge\"," API_V1_SCHEMA_CHARGE_FIELDS ",\"refused_while\":\"in_throw\","
    "\"about\":\"what the next throw uses, either field or both; one target for the unit: the menu's digits show "
    "it\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/charge/start\"," API_V1_SCHEMA_CHARGE_FIELDS ","
    "\"refused_while\":\"a_job_runs\",\"about\":\"starts a charge (202); a target or a powder profile given is set "
    "only if the start is taken. 409 busy also while a person has a screen open at the unit; 409 driver_fault; 400 "
    "no_target when none is set\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/charge/stop\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"stops the charge (202), or 200 with nothing to stop; it acknowledges the charge's fault\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/purge/start\",\"fields\":[],\"refused_while\":\"a_job_runs\","
    "\"about\":\"starts a purge (202), at speed 0 until a PATCH gives one; 409 driver_fault\"}",
    "{\"method\":\"PATCH\",\"path\":\"/api/v1/purge\",\"fields\":[{\"name\":\"speed_rps\",\"type\":\"number\","
    "\"unit\":\"rev/s\",\"required\":true,\"about\":\"both tricklers; beyond plus or minus the lower of the two "
    "motors' max_speed_rps, turned at it: the reply gives the speed taken\"},"
    "{\"name\":\"time_s\",\"type\":\"number\",\"unit\":\"s\",\"min\":1,\"max\":600,"
    "\"about\":\"for at most this long, then the purge stops by itself; without it, until stopped. A client with no "
    "person at the unit gives a short time and sends the speed again while it wants it\"}],"
    "\"refused_while\":\"no_purge_runs\",\"about\":\"the purge's speed; 409 not_running when none runs, and in its "
    "fault, where the motors stay stopped until it is acknowledged\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/purge/stop\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"stops the purge (202), or 200 with nothing to stop; it acknowledges the purge's fault\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/stop\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"stops whatever job runs, or a start asked and not yet begun; never fails\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/scale/zero\",\"fields\":[],\"about\":\"asks the scale to zero, not "
    "that it did. Taken with no job, in a purge and in a fault; in zeroing only while the weight is within 5 times "
    "zero_mean_margin of zero (409 weight_off_zero, with the weight); 409 busy while the charge pours or waits for the "
    "cup; 501 not_supported where the make cannot (GET /api/v1/scale, actions)\"}",
    "{\"method\":\"PATCH\",\"path\":\"/api/v1/gate\",\"fields\":[{\"name\":\"ratio\",\"type\":\"number\",\"min\":0,"
    "\"max\":1,\"required\":true,\"about\":\"0 open, 1 closed\"}],\"refused_while\":\"in_throw\","
    "\"about\":\"moves the servo gate; 409 gate_disabled when it is off in settings/gate\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/update/start\",\"fields\":[{\"name\":\"download\","
    "\"type\":\"boolean\",\"about\":\"true: the job fetches the release the poll found itself (found in GET "
    "/api/v1/update), checks it and installs it, as the offer's Install at the screen does\"}],"
    "\"refused_while\":\"a_job_runs\",\"about\":\"starts a firmware update (202): the menu takes it and it holds the "
    "screen; then PUT the image, unless it downloads. 409 busy also while a person has a screen open at the unit or a "
    "poll runs; 409 not_found for a download with nothing found\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/update/check\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"polls the update's source now (202): GET /api/v1/update's poll and found say what came of it. It also "
    "polls once each start, when the station joins. 409 busy while a poll or the update runs; 409 no_network on the "
    "unit's own access point\"}",
    "{\"method\":\"PUT\",\"path\":\"/api/v1/update/image\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"the update image (app.bin of the image GET /api/v1/update takes), application/octet-stream: written "
    "into the free slot as it comes, then checked whole (200, ready). 409 not_running unless receiving; 413 too_large; "
    "400 not_an_image, wrong_chip, wrong_board, not_an_update, not_newer, wrong_slot or hash_mismatch, which end the "
    "job with nothing installed\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/update/install\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"restarts into a trial of the image (202); the unit comes back on it, confirmed once on its network, "
    "or on the one before. 409 not_ready unless ready\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/update/stop\",\"fields\":[],\"refused_while\":\"never\","
    "\"about\":\"stops the update (202), or 200 with nothing to stop; the image written is never booted\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/system/restart\",\"fields\":[],\"refused_while\":\"a_job_runs\","
    "\"about\":\"restarts the unit once the reply is out (202)\"}",
    "{\"method\":\"POST\",\"path\":\"/api/v1/system/factory_reset\",\"fields\":[{\"name\":\"confirm\",\"type\":\"choice\","
    "\"choices\":[{\"value\":\"factory_reset\",\"label\":\"Factory reset\"}],\"required\":true}],\"refused_while\":\"a_job_runs\","
    "\"about\":\"erases the stored settings and restarts (202): the unit then leaves the network and comes back as "
    "its own access point, with a new id and host name. 400 confirm_missing without the confirmation\"}",
};

static const char *const api_v1_schema_states[] = {
    "\"activity\":[{\"value\":\"none\",\"about\":\"no job: the menu's forms\"},{\"value\":\"charge\"},"
    "{\"value\":\"purge\"},{\"value\":\"view\",\"about\":\"a person has a screen open at the unit that is not a job (the Wi-Fi "
    "information, a driver's fault): a start is refused until it is closed there\"},{\"value\":\"update\","
    "\"about\":\"a firmware update (ADR-004)\"}]",
    "\"update\":[{\"value\":\"idle\"},{\"value\":\"starting\",\"about\":\"asked, and the menu has not begun it "
    "yet\"},{\"value\":\"receiving\",\"about\":\"the image is taken into the free slot\"},{\"value\":\"ready\","
    "\"about\":\"the image is whole and checked: install, or stop\"},{\"value\":\"installing\",\"about\":\"the "
    "restart into its trial is under way\"},{\"value\":\"ending\",\"about\":\"ended: the screen is given back, "
    "and a start is taken again once it is idle\"}]",
    "\"charge\":[{\"value\":\"idle\"},{\"value\":\"starting\",\"about\":\"asked, and the menu has not begun it "
    "yet\"},{\"value\":\"zeroing\",\"about\":\"a throw waits for the scale to read zero, steady\"},"
    "{\"value\":\"charging\",\"about\":\"a throw pours to the target\"},{\"value\":\"waiting_for_cup_removal\","
    "\"about\":\"the throw's verdict is given: the cup is to be lifted\"},{\"value\":\"waiting_for_cup_return\","
    "\"about\":\"the cup is away; the next throw begins when it is back\"},{\"value\":\"fault\",\"about\":\"the "
    "charge ended on a fault (last.reason) and waits for a stop or a press to acknowledge it\"}]",
    "\"purge\":[{\"value\":\"idle\"},{\"value\":\"starting\",\"about\":\"asked, and the menu has not begun it "
    "yet\"},{\"value\":\"running\"},{\"value\":\"fault\",\"about\":\"ended on a driver's fault, waiting for a stop or "
    "a press\"}]",
    "\"gate\":[{\"value\":\"open\"},{\"value\":\"closed\"},{\"value\":\"partial\"},{\"value\":\"held\",\"about\":"
    "\"the menu's Disable: the servo is moved no more and stays at its last ratio until charge mode moves it\"},"
    "{\"value\":\"disabled\",\"about\":\"off in settings/gate: charge mode does not use it\"}]",
    "\"driver\":[{\"value\":\"ok\"},{\"value\":\"absent\",\"about\":\"the driver does not answer on its serial "
    "line\"},{\"value\":\"short\"},{\"value\":\"hot\"},{\"value\":\"reset\",\"about\":\"the driver restarted and "
    "lost its set-up\"},{\"value\":\"supply\",\"about\":\"its motor supply is too low\"}]",
};

static const char *const api_v1_schema_events[] = {
    "{\"source\":\"charge\",\"kind\":\"started\",\"carries\":[\"by\",\"target\",\"powder_profile\"],"
    "\"about\":\"by: button, api, console\"}",
    "{\"source\":\"charge\",\"kind\":\"stopped\",\"carries\":[\"by\",\"reason\"],\"about\":\"by: button, api, "
    "console, or unit when it ended itself; reason: stopped, fault\"}",
    "{\"source\":\"throw\",\"kind\":\"started\",\"carries\":[\"throw\",\"target\",\"powder_profile\"],"
    "\"about\":\"throw: its number, from 1 at each start of the unit\"}",
    "{\"source\":\"throw\",\"kind\":\"paused\",\"carries\":[\"throw\",\"pause\"],\"about\":\"no reading for 3 of the "
    "scale's intervals; pause: its number in the throw\"}",
    "{\"source\":\"throw\",\"kind\":\"resumed\",\"carries\":[\"throw\",\"weight\"],\"about\":\"weight: the settled "
    "weight it went on from\"}",
    "{\"source\":\"throw\",\"kind\":\"finished\",\"carries\":[\"throw\",\"result\",\"weight\",\"elapsed_s\"],"
    "\"about\":\"the verdict; result: good, under, over\"}",
    "{\"source\":\"throw\",\"kind\":\"ended\",\"carries\":[\"throw\",\"reason\",\"motor\",\"fault\"],\"about\":\"a "
    "throw cut short; reason: stopped, scale_no_reading, scale_unsteady, scale_unreliable, driver_fault; motor and "
    "fault only with driver_fault\"}",
    "{\"source\":\"purge\",\"kind\":\"started\",\"carries\":[\"by\"]}",
    "{\"source\":\"purge\",\"kind\":\"stopped\",\"carries\":[\"by\",\"reason\"],\"about\":\"reason: stopped, time "
    "(its time ran out: by the unit), fault\"}",
    "{\"source\":\"motors\",\"kind\":\"fault\",\"carries\":[\"motor\",\"fault\"],\"about\":\"a driver's fault "
    "appeared; motor: coarse, fine; fault: as the driver states\"}",
    "{\"source\":\"motors\",\"kind\":\"cleared\",\"carries\":[\"motor\",\"fault\"],\"about\":\"fault: the one that "
    "cleared\"}",
    "{\"source\":\"system\",\"kind\":\"started\",\"carries\":[\"firmware\"],\"about\":\"the unit started; the first "
    "event after each start\"}",
    "{\"source\":\"update\",\"kind\":\"started\",\"carries\":[\"by\"]}",
    "{\"source\":\"update\",\"kind\":\"stopped\",\"carries\":[\"by\",\"reason\",\"code\"],\"about\":\"reason: "
    "stopped, refused (with the refusal's code), time\"}",
    "{\"source\":\"update\",\"kind\":\"received\",\"carries\":[\"version\"],\"about\":\"the image is whole and "
    "checked\"}",
    "{\"source\":\"update\",\"kind\":\"installing\",\"carries\":[\"version\"]}",
    "{\"source\":\"update\",\"kind\":\"confirmed\",\"carries\":[\"version\"],\"about\":\"after the restart, the "
    "image tried confirmed itself once on its network\"}",
    "{\"source\":\"update\",\"kind\":\"back\",\"carries\":[\"version\"],\"about\":\"after the restart, the image "
    "tried was left, and the one before runs\"}",
    "{\"source\":\"update\",\"kind\":\"found\",\"carries\":[\"version\"],\"about\":\"the poll found a newer release, "
    "now offered: GET /api/v1/update, found\"}",
};


const char *api_v1_schema_item(api_v1_schema_part_t part, size_t idx) {
    switch (part) {
        case API_V1_SCHEMA_ACTIONS:
            return idx < sizeof(api_v1_schema_actions) / sizeof(api_v1_schema_actions[0]) ?
                   api_v1_schema_actions[idx] : NULL;
        case API_V1_SCHEMA_STATES:
            return idx < sizeof(api_v1_schema_states) / sizeof(api_v1_schema_states[0]) ?
                   api_v1_schema_states[idx] : NULL;
        case API_V1_SCHEMA_EVENTS:
        default:
            return idx < sizeof(api_v1_schema_events) / sizeof(api_v1_schema_events[0]) ?
                   api_v1_schema_events[idx] : NULL;
    }
}


bool api_v1_commands_init(void) {
    bool ok = true;
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/charge", api_v1_get_charge);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/charge", api_v1_patch_charge);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/charge/start", api_v1_charge_start);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/charge/stop", api_v1_charge_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/purge", api_v1_get_purge);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/purge", api_v1_patch_purge);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/purge/start", api_v1_purge_start);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/purge/stop", api_v1_purge_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/stop", api_v1_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/events", api_v1_get_events);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/scale", api_v1_get_scale);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/scale/zero", api_v1_scale_zero);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/gate", api_v1_get_gate);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/gate", api_v1_patch_gate);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/motors", api_v1_get_motors);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/system", api_v1_get_system);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/system/restart", api_v1_system_restart);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/system/factory_reset", api_v1_system_factory_reset);
    // Hidden: in neither the index nor the schema (the owner's hello for agents)
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/agent/hello", api_v1_agent_hello);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/network", api_v1_get_network);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/update", api_v1_get_update);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/update/start", api_v1_update_start);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/update/check", api_v1_update_check);
    ok = ok && http_server_add_upload(HTTP_SERVER_PUT, "/api/v1/update/image", &api_v1_update_image);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/update/install", api_v1_update_install);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/update/stop", api_v1_update_stop);
    return ok;
}
