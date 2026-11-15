/*
 * API v1's commands and the unit's reads (ADR-003, gate 4; api_v1.md): the charge, the cleanup, the stop of either,
 * the event log, the scale and its zero, the gate, the motors, the system with its restart and erase, the network.
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


// 409 busy, with the activity and the job's state
static void api_busy(http_server_reply_t *reply, const char *message) {
    char extra[96];
    charge_status_t charge;
    charge_mode_get_status(&charge);
    activity_t activity = activity_get();
    const char *state = "idle";
    if (activity == ACTIVITY_CHARGE) {
        state = charge.state;
    }
    else if (activity == ACTIVITY_CLEANUP) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        state = cleanup.state;
    }
    snprintf(extra, sizeof(extra), "\"activity\":\"%s\",\"state\":\"%s\"", activity_name(activity), state);
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
    activity_t asked;
    if (activity_get() != ACTIVITY_NONE || activity_start_pending(&asked) || menu_get_current_form_id() < 0) {
        api_busy(reply, "another job runs, a start is asked already, or a person has a screen open at the unit");
        return;
    }
    if (motors_get_fault() != NULL) {
        api_driver_fault(reply);
        return;
    }
    if (!has_target && charge_mode_get_target() <= 0.0f) {
        api_error(reply, 400, "no_target", "target", "no target is set: give one", NULL);
        return;
    }
    if (has_target) {
        charge_mode_set_target(target);
    }
    if (has_profile) {
        profile_select((uint8_t) profile);
    }
    switch (charge_mode_request_start(ACTIVITY_BY_API)) {
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
            api_busy(reply, "another job runs, or a person has a screen open at the unit");
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


// --- The cleanup

static void reply_cleanup(http_server_reply_t *reply, int status) {
    cleanup_status_t cleanup;
    cleanup_mode_get_status(&cleanup);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"state\":");
    http_json_string(out, room, &len, cleanup.state);
    http_json_raw(out, room, &len, ",\"speed_rps\":");
    http_json_float(out, room, &len, cleanup.speed_rps);
    http_json_raw(out, room, &len, ",\"time_left_s\":");
    http_json_number(out, room, &len, cleanup.time_left_s, 4);
    http_json_raw(out, room, &len, "}");
    api_send(reply, status, room, len);
}


static void api_v1_get_cleanup(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    reply_cleanup(reply, 200);
}


// PATCH /api/v1/cleanup: {"speed_rps": 2.0}, or with "time_s", at most that long
static void api_v1_patch_cleanup(const http_server_request_t *request, http_server_reply_t *reply) {
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
        api_error(reply, 409, "not_running", NULL, "no cleanup runs: start one first", NULL);
        return;
    }
    reply_cleanup(reply, 200);
}


static void api_v1_cleanup_start(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    switch (cleanup_mode_request_start(ACTIVITY_BY_API)) {
        case CLEANUP_START_OK:
            reply_cleanup(reply, 202);
            return;
        case CLEANUP_START_DRIVER_FAULT:
            api_driver_fault(reply);
            return;
        default:
            api_busy(reply, "another job runs, a start is asked already, or a person has a screen open at the unit");
            return;
    }
}


static void api_v1_cleanup_stop(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    reply_cleanup(reply, activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_API) ? 202 : 200);
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
        http_json_integer(buf, room, len, (long) api_v1_boot_id());
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
        if (events_latest_seq() <= after) {
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
    if (activity == ACTIVITY_SCALE_CALIBRATION) {
        refused = "the scale is being calibrated";
    }
    else if (activity == ACTIVITY_CHARGE && strcmp(charge.state, "fault") != 0) {
        if (strcmp(charge.state, "zeroing") != 0) {
            refused = "a throw pours, or the unit waits for the cup: a zero now would start a throw on whatever is "
                      "on the pan";
        }
        else if (!(fabsf(weight) <= band)) {
            off_zero = true;
        }
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


static void api_v1_system_restart(const http_server_request_t *request, http_server_reply_t *reply) {
    if (!api_empty(request, reply)) {
        return;
    }
    if (activity_get() == ACTIVITY_CHARGE || activity_get() == ACTIVITY_CLEANUP ||
        activity_get() == ACTIVITY_SCALE_CALIBRATION) {
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


static void api_v1_system_erase(const http_server_request_t *request, http_server_reply_t *reply) {
    static const char *const allowed[] = {"confirm"};
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    if (!api_body(request, reply, members, &count) || !api_only(reply, members, count, allowed, 1)) {
        return;
    }
    const http_json_member_t *confirm = api_member(members, count, "confirm");
    char text[16];
    if (confirm == NULL || confirm->kind != HTTP_JSON_STRING || !http_json_string_value(confirm, text, sizeof(text)) ||
        strcmp(text, "erase") != 0) {
        api_error(reply, 400, "confirm_missing", "confirm", "send {\"confirm\": \"erase\"}: it cannot be undone", NULL);
        return;
    }
    if (activity_get() == ACTIVITY_CHARGE || activity_get() == ACTIVITY_CLEANUP ||
        activity_get() == ACTIVITY_SCALE_CALIBRATION) {
        api_busy(reply, "a job runs: stop it first");
        return;
    }
    if (!api_v1_restart_later(true)) {
        http_server_reply_error(reply, 500, "failed", "the erase could not be set going");
        return;
    }
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"erasing\":true}");
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


bool api_v1_commands_init(void) {
    bool ok = true;
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/charge", api_v1_get_charge);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/charge", api_v1_patch_charge);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/charge/start", api_v1_charge_start);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/charge/stop", api_v1_charge_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/cleanup", api_v1_get_cleanup);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/cleanup", api_v1_patch_cleanup);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/cleanup/start", api_v1_cleanup_start);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/cleanup/stop", api_v1_cleanup_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/stop", api_v1_stop);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/events", api_v1_get_events);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/scale", api_v1_get_scale);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/scale/zero", api_v1_scale_zero);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/gate", api_v1_get_gate);
    ok = ok && http_server_add_route(HTTP_SERVER_PATCH, "/api/v1/gate", api_v1_patch_gate);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/motors", api_v1_get_motors);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/system", api_v1_get_system);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/system/restart", api_v1_system_restart);
    ok = ok && http_server_add_route(HTTP_SERVER_POST, "/api/v1/system/erase", api_v1_system_erase);
    ok = ok && http_server_add_route(HTTP_SERVER_GET, "/api/v1/network", api_v1_get_network);
    return ok;
}
