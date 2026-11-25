/*
 * ADR-009 step 1: the calibration as a job on the unit (calibration.h). The method is calibration_method.c's; this
 * is its bench on the unit (the motors, the scale, the job's waits), the person's steps, and the records' keeping.
 */
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/time.h"

#include "activity.h"
#include "calibration.h"
#include "calibration_method.h"
#include "charge_mode.h"
#include "events.h"
#include "jobs.h"
#include "menu.h"
#include "motors.h"
#include "profile.h"
#include "scale.h"
#include "settings.h"

#define CALIBRATION_POLL_MS 50          // how often a wait looks for an answer, a stop and the drivers
#define CALIBRATION_READING_WAIT_MS 200 // a watch waits for a reading this long at most before it looks again
#define CALIBRATION_ZERO_WAIT_MS 8000   // the zero asked: steady near 0 within this, or a failure
#define CALIBRATION_STEADY_READINGS 5   // readings that are steady: within two steps of their median
#define CALIBRATION_STEADY_GR 0.04f
#define CALIBRATION_LIFTED_GR 2.0f      // the cup lifted: the reading this far below zero, and steady
#define CALIBRATION_BACK_GR 0.5f        // put back: within this of zero, and steady

extern charge_mode_config_t charge_mode_config;
extern scale_config_t scale_config;

// The job's state, read by the API, the console and the screen under the activity's lock
static volatile calibration_part_t asked_part = CALIBRATION_PART_NONE;
static volatile int asked_profile = -1;
static calibration_status_t status;    // its state NULL until the first part: idle
static volatile bool running = false;

// The person's answer, given from another task, taken by the job
typedef enum {
    ANSWER_NONE = 0,
    ANSWER_NEXT,
    ANSWER_FULL,
    ANSWER_KEEP,
    ANSWER_DONE,
    ANSWER_GO_ON,
    ANSWER_SAVE,
} answer_t;
static volatile answer_t answer_given = ANSWER_NONE;
static volatile activity_by_t answer_by = ACTIVITY_BY_BUTTON;     // who gave it: the API, the console or a button

// How the job ended: who stopped it, and why; ending once a stop or a fault was taken
static activity_by_t end_by = ACTIVITY_BY_BUTTON;
static event_reason_t end_reason = EVENT_REASON_STOPPED;
static bool ending = false;


const char *calibration_part_name(calibration_part_t part) {
    static const char *const names[] = {NULL, "cup", "scale", "powder"};
    return part <= CALIBRATION_PART_POWDER ? names[part] : NULL;
}


const char *calibration_step_name(calibration_step_t step) {
    static const char *const names[] = {NULL, "cup_on", "zeroing", "lift", "back", "fill", "keep", "empty",
                                        "running", "paused", "result", "failed"};
    return step <= CALIBRATION_STEP_FAILED ? names[step] : NULL;
}


calibration_part_t calibration_part_from_name(const char *name) {
    for (calibration_part_t part = CALIBRATION_PART_CUP; part <= CALIBRATION_PART_POWDER; part += 1) {
        if (name != NULL && strcmp(name, calibration_part_name(part)) == 0) {
            return part;
        }
    }
    return CALIBRATION_PART_NONE;
}


static void calibration_log(event_kind_t kind, activity_by_t by, event_reason_t reason, float value) {
    event_t event = {.source = EVENT_SOURCE_CALIBRATION, .kind = (uint8_t) kind, .by = (uint8_t) by,
                     .reason = (uint8_t) reason, .value = value, .text = calibration_part_name(status.part)};
    events_add(&event);
}


static void set_step(calibration_step_t step, const char *line) {
    activity_lock();
    status.step = step;
    if (line != NULL) {
        snprintf(status.line, sizeof(status.line), "%s", line);
    }
    activity_unlock();
    menu_wake();
}


// A stop asked, or a motor driver not OK: the job ends. True when it is to end
static bool calibration_should_end(void) {
    activity_by_t by;
    if (ending) {
        return true;
    }
    if (activity_take_stop(&by)) {
        end_by = by;
        end_reason = EVENT_REASON_STOPPED;
        ending = true;
    }
    else if (motors_get_fault() != NULL) {
        end_by = ACTIVITY_BY_UNIT;
        end_reason = EVENT_REASON_FAULT;
        ending = true;
    }
    return ending;
}


// Wait for the person's answer, one of two; the answer taken, or ANSWER_NONE when the job is to end
static answer_t wait_answer(answer_t expected, answer_t also) {
    while (true) {
        if (calibration_should_end()) {
            return ANSWER_NONE;
        }
        activity_lock();
        answer_t given = answer_given;
        answer_given = ANSWER_NONE;
        activity_unlock();
        if (given == expected || (also != ANSWER_NONE && given == also)) {
            return given;
        }
        jobs_wait(CALIBRATION_POLL_MS);
    }
}


// The latest few readings, to tell when the scale is steady
static float recent[CALIBRATION_STEADY_READINGS];
static int recent_count = 0;

static void recent_add(float weight) {
    memmove(recent, recent + 1, sizeof(float) * (CALIBRATION_STEADY_READINGS - 1));
    recent[CALIBRATION_STEADY_READINGS - 1] = weight;
    if (recent_count < CALIBRATION_STEADY_READINGS) {
        recent_count += 1;
    }
}


// Steady: the last readings within CALIBRATION_STEADY_GR of each other; their middle into *weight
static bool recent_steady(float *weight) {
    if (recent_count < CALIBRATION_STEADY_READINGS) {
        return false;
    }
    float low = recent[0];
    float high = recent[0];
    for (int idx = 1; idx < CALIBRATION_STEADY_READINGS; idx += 1) {
        low = fminf(low, recent[idx]);
        high = fmaxf(high, recent[idx]);
    }
    *weight = (low + high) / 2.0f;
    return high - low <= CALIBRATION_STEADY_GR;
}


/*
 * Wait until the reading, steady, meets a test (NULL: any steady reading); within timeout_ms (0: no end). The
 * reading into *weight. False when the job is to end or the time ran out (then *weight is NAN)
 */
static bool wait_steady(bool (*test)(float weight), uint32_t timeout_ms, float *weight) {
    uint64_t start = time_us_64();
    recent_count = 0;
    *weight = NAN;
    while (true) {
        if (calibration_should_end()) {
            return false;
        }
        float reading;
        if (scale_block_wait_for_next_measurement(CALIBRATION_READING_WAIT_MS, &reading) && isfinite(reading)) {
            recent_add(reading);
            float steady;
            if (recent_steady(&steady) && (test == NULL || test(steady))) {
                *weight = steady;
                return true;
            }
        }
        if (timeout_ms > 0 && time_us_64() - start > (uint64_t) timeout_ms * 1000u) {
            return false;
        }
    }
}


static bool near_zero(float weight) {
    return fabsf(weight) <= CALIBRATION_STEADY_GR;
}


static bool lifted(float weight) {
    return weight <= -CALIBRATION_LIFTED_GR;
}


static bool put_back(float weight) {
    return fabsf(weight) <= CALIBRATION_BACK_GR;
}


// --- The method's bench on the unit

static motor_select_t motor_of(int tube) {
    return tube == CALIBRATION_FINE ? SELECT_FINE_TRICKLER_MOTOR : SELECT_COARSE_TRICKLER_MOTOR;
}


static void bench_set_speed(void *ctx, int tube, float rps) {
    (void) ctx;
    motor_set_speed(motor_of(tube), rps);
}


static int bench_watch(void *ctx, float seconds, calibration_sample_t *out, int room) {
    (void) ctx;
    uint64_t start = time_us_64();
    uint64_t end = start + (uint64_t) (seconds * 1e6f);
    int count = 0;
    while (true) {
        if (calibration_should_end()) {
            return -1;
        }
        uint64_t now = time_us_64();
        if (now >= end) {
            return count;
        }
        uint32_t wait_ms = (uint32_t) ((end - now) / 1000u);
        if (wait_ms > CALIBRATION_READING_WAIT_MS) {
            wait_ms = CALIBRATION_READING_WAIT_MS;
        }
        float reading;
        if (scale_block_wait_for_next_measurement(wait_ms > 0 ? wait_ms : 1, &reading) && isfinite(reading) &&
            count < room) {
            out[count].t_s = (float) (time_us_64() - start) / 1e6f;
            out[count].weight = reading;
            count += 1;
        }
    }
}


static bool bench_cup_full(void *ctx, float held, float most) {
    (void) ctx;
    char line[40];
    snprintf(line, sizeof(line), "It holds %.0f of %.0f", (double) held, (double) (held + most));
    calibration_step_t was = status.step;
    set_step(CALIBRATION_STEP_PAUSED, line);
    calibration_log(EVENT_PAUSED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, held);
    if (wait_answer(ANSWER_GO_ON, ANSWER_NONE) == ANSWER_NONE) {
        return false;
    }
    calibration_log(EVENT_RESUMED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, 0.0f);
    set_step(was, "");
    return true;
}


static void bench_report(void *ctx, int percent, const char *line) {
    (void) ctx;
    activity_lock();
    status.progress = percent;
    snprintf(status.line, sizeof(status.line), "%s", line);
    activity_unlock();
}


static calibration_bench_t unit_bench = {
    .ctx = NULL,
    .set_speed = bench_set_speed,
    .watch = bench_watch,
    .cup_full = bench_cup_full,
    .report = bench_report,
};


// --- The parts

static void fail(const char *failure);

// The empty cup on the scale, and the scale zeroed with it. False when the job is to end or failed
static bool cup_on_and_zero(void) {
    set_step(CALIBRATION_STEP_CUP_ON, "Put the empty cup on the scale");
    if (wait_answer(ANSWER_NEXT, ANSWER_NONE) == ANSWER_NONE) {
        return false;
    }
    set_step(CALIBRATION_STEP_ZEROING, "Zeroing");
    scale_ask_zero();
    float weight;
    if (!wait_steady(near_zero, CALIBRATION_ZERO_WAIT_MS, &weight)) {
        if (!ending) {
            fail("Scale: no steady zero");
        }
        return false;
    }
    return true;
}


// What the scale's settings hold, as the method takes it
static void scale_records(calibration_scale_record_t *record) {
    memset(record, 0, sizeof(*record));
    const eeprom_scale_data_t *stored = &scale_config.persistent_config;
    record->settle_s = stored->settle_ms >= 0.0f ? stored->settle_ms / 1000.0f : NAN;
    for (int idx = 0; idx < SCALE_LAG_POINTS && idx < CALIBRATION_LAG_POINTS; idx += 1) {
        if (stored->lag[idx].flow_per_s >= 0.0f && stored->lag[idx].lag_ms >= 0.0f) {
            record->lag_flow_per_s[record->lag_count] = stored->lag[idx].flow_per_s;
            record->lag_s[record->lag_count] = stored->lag[idx].lag_ms / 1000.0f;
            record->lag_count += 1;
        }
    }
}


static void fail(const char *failure) {
    activity_lock();
    snprintf(status.failure, sizeof(status.failure), "%s", failure);
    end_reason = EVENT_REASON_FAILED;
    activity_unlock();
    set_step(CALIBRATION_STEP_FAILED, failure);
    wait_answer(ANSWER_DONE, ANSWER_NONE);
}


static void run_cup(void) {
    if (!cup_on_and_zero()) {
        return;
    }
    // Lifted: its weight is how far the reading falls
    set_step(CALIBRATION_STEP_LIFT, "Lift the cup");
    float weight;
    if (!wait_steady(lifted, 0, &weight)) {
        return;
    }
    activity_lock();
    status.cup_weight = -weight;
    activity_unlock();
    set_step(CALIBRATION_STEP_BACK, "Put the cup back");
    if (!wait_steady(put_back, 0, &weight)) {
        return;
    }
    // Filled by the person, the tubes' speeds set from the screen, the API or the console
    activity_lock();
    status.fine_rps = 0.0f;
    status.coarse_rps = 0.0f;
    activity_unlock();
    set_step(CALIBRATION_STEP_FILL, "Fill the cup");
    answer_t given = wait_answer(ANSWER_FULL, ANSWER_NONE);
    motor_set_speed(SELECT_BOTH_MOTOR, 0.0f);
    activity_lock();
    status.fine_rps = 0.0f;
    status.coarse_rps = 0.0f;
    activity_unlock();
    if (given == ANSWER_NONE) {
        return;
    }
    if (!wait_steady(NULL, 0, &weight)) {
        return;
    }
    activity_lock();
    status.capacity = weight;
    activity_unlock();
    set_step(CALIBRATION_STEP_KEEP, "Keep it as its capacity?");
    if (wait_answer(ANSWER_KEEP, ANSWER_NONE) == ANSWER_NONE) {
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%.2f", (double) status.capacity);
    activity_lock();
    activity_by_t kept_by = answer_by;      // who answered Keep
    activity_unlock();
    const char *why = settings_write_by("settings/charge", "cup_capacity", text, kept_by);
    snprintf(text, sizeof(text), "%.2f", (double) status.cup_weight);
    why = why != NULL ? why : settings_write_by("settings/charge", "cup_weight", text, kept_by);
    why = why != NULL ? why : settings_store_by("settings/charge", kept_by);
    if (why != NULL) {
        printf("Calibration: the cup not kept, %s\n", why);
        fail("Cup: not kept");
        return;
    }
    calibration_log(EVENT_STORED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, status.capacity);
    set_step(CALIBRATION_STEP_EMPTY, "Empty the cup");
    wait_answer(ANSWER_DONE, ANSWER_NONE);
}


// The cup's capacity as stored, and the powder in it now, for the method's pauses
static void cup_for_method(void) {
    float capacity = charge_mode_config.eeprom_charge_mode_data.cup_capacity;
    calibration_cup(capacity > 0.0f ? capacity : NAN, scale_get_current_measurement());
}


static bool wait_result(void) {
    set_step(CALIBRATION_STEP_RESULT, "");
    calibration_log(EVENT_FINISHED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, 0.0f);
    return wait_answer(ANSWER_SAVE, ANSWER_NONE) == ANSWER_SAVE;
}


static void run_scale(void) {
    if (!cup_on_and_zero()) {
        return;
    }
    cup_for_method();
    set_step(CALIBRATION_STEP_RUNNING, "");
    calibration_scale_record_t record;
    if (calibration_scale_part(&unit_bench, &record) == CALIBRATION_STOPPED) {
        return;
    }
    activity_lock();
    status.scale = record;
    activity_unlock();
    if (isnan(record.settle_s) && record.lag_count == 0) {
        fail(record.failure[0] != '\0' ? record.failure : "Scale: nothing measured");
        return;
    }
    wait_result();      // kept by calibration_save(), from the result step
}


static void run_powder(void) {
    if (!cup_on_and_zero()) {
        return;
    }
    cup_for_method();
    const profile_t *profile = profile_get((uint16_t) status.profile);
    activity_lock();
    status.was_fine_rps = profile->fine_min_flow_speed_rps;
    status.was_coarse_rps = profile->coarse_min_flow_speed_rps;
    activity_unlock();
    calibration_scale_record_t scale;
    scale_records(&scale);
    set_step(CALIBRATION_STEP_RUNNING, "");
    calibration_speeds_record_t fine;
    calibration_speeds_record_t coarse;
    if (calibration_speeds_part(&unit_bench, CALIBRATION_FINE, get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR),
                                get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR), &scale, &fine) ==
        CALIBRATION_STOPPED) {
        return;
    }
    if (calibration_speeds_part(&unit_bench, CALIBRATION_COARSE, get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR), &scale, &coarse) ==
        CALIBRATION_STOPPED) {
        return;
    }
    activity_lock();
    status.fine = fine;
    status.coarse = coarse;
    activity_unlock();
    if (isnan(fine.least_rps) && isnan(coarse.least_rps)) {
        fail(fine.failure[0] != '\0' ? fine.failure : coarse.failure);
        return;
    }
    wait_result();
}


// The room the method keeps a run's readings in, from the heap while a part runs: false when there is none
static bool bench_room(bool take) {
    if (!take) {
        vPortFree(unit_bench.during);
        unit_bench.during = NULL;
        unit_bench.after = NULL;
        return true;
    }
    unit_bench.during = pvPortMalloc(sizeof(calibration_sample_t) * (CALIBRATION_DURING_MAX + CALIBRATION_AFTER_MAX));
    unit_bench.after = unit_bench.during != NULL ? unit_bench.during + CALIBRATION_DURING_MAX : NULL;
    return unit_bench.during != NULL;
}


uint8_t calibration_job_run(void) {
    if (!motors_ready()) {
        printf("Calibration: not started, %s\n", motors_get_fault());
        return JOBS_EXIT_MOTOR_FAULT;
    }
    activity_lock();
    calibration_part_t part = asked_part;
    memset(&status, 0, sizeof(status));
    status.state = "running";
    status.part = part;
    status.profile = part == CALIBRATION_PART_POWDER ?
                     (asked_profile >= 0 ? asked_profile : (int) profile_get_selected_idx()) : -1;
    status.capacity = NAN;
    status.cup_weight = NAN;
    status.scale.settle_s = NAN;
    status.fine.least_rps = NAN;
    status.coarse.least_rps = NAN;
    status.was_fine_rps = NAN;
    status.was_coarse_rps = NAN;
    answer_given = ANSWER_NONE;
    running = true;
    activity_unlock();
    activity_job_begin(ACTIVITY_CALIBRATION);
    status.by = activity_by_name(activity_job_by());
    end_by = ACTIVITY_BY_UNIT;
    end_reason = EVENT_REASON_FINISHED;
    ending = false;
    calibration_log(EVENT_STARTED, activity_job_by(), EVENT_REASON_NONE, 0.0f);
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);

    if (part != CALIBRATION_PART_CUP && !bench_room(true)) {
        fail("No memory for the runs");
    }
    else {
        switch (part) {
            case CALIBRATION_PART_CUP:
                run_cup();
                break;
            case CALIBRATION_PART_SCALE:
                run_scale();
                break;
            case CALIBRATION_PART_POWDER:
                run_powder();
                break;
            default:
                break;
        }
        bench_room(false);
    }

    motor_set_speed(SELECT_BOTH_MOTOR, 0.0f);
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);
    calibration_log(EVENT_STOPPED, end_by, end_reason, 0.0f);
    activity_lock();
    running = false;
    status.state = "idle";
    status.part = CALIBRATION_PART_NONE;
    status.profile = -1;
    status.step = CALIBRATION_STEP_NONE;
    activity_unlock();
    activity_job_end();
    return JOBS_EXIT_MAIN_MENU;
}


calibration_start_result_t calibration_request_start(calibration_part_t part, int profile, activity_by_t by) {
    activity_t asked_before;
    if (part == CALIBRATION_PART_NONE || activity_get() != ACTIVITY_NONE || activity_start_pending(&asked_before) ||
        menu_get_current_form_id() < 0) {
        return CALIBRATION_START_BUSY;
    }
    if (!motors_ready()) {
        return CALIBRATION_START_DRIVER_FAULT;
    }
    asked_part = part;
    asked_profile = profile_get(profile >= 0 ? (uint16_t) profile : 0xFFFFu) != NULL ? profile : -1;
    if (!activity_request_start(ACTIVITY_CALIBRATION, by)) {
        return CALIBRATION_START_BUSY;
    }
    menu_wake();
    return CALIBRATION_START_OK;
}


calibration_answer_result_t calibration_answer(const char *answer, activity_by_t by) {
    static const struct {
        const char *word;
        answer_t answer;
        calibration_step_t step;
    } answers[] = {
        {"next", ANSWER_NEXT, CALIBRATION_STEP_CUP_ON},
        {"full", ANSWER_FULL, CALIBRATION_STEP_FILL},
        {"keep", ANSWER_KEEP, CALIBRATION_STEP_KEEP},
        {"done", ANSWER_DONE, CALIBRATION_STEP_EMPTY},
        {"done", ANSWER_DONE, CALIBRATION_STEP_FAILED},
        {"go_on", ANSWER_GO_ON, CALIBRATION_STEP_PAUSED},
    };
    calibration_answer_result_t result = CALIBRATION_ANSWER_NOT_NOW;
    activity_lock();
    if (!running) {
        result = CALIBRATION_ANSWER_NO_JOB;
    }
    else {
        for (size_t idx = 0; idx < sizeof(answers) / sizeof(answers[0]); idx += 1) {
            if (answer != NULL && strcmp(answer, answers[idx].word) == 0 && status.step == answers[idx].step) {
                answer_given = answers[idx].answer;
                answer_by = by;
                result = CALIBRATION_ANSWER_OK;
                break;
            }
        }
    }
    activity_unlock();
    if (result == CALIBRATION_ANSWER_OK) {
        jobs_wake();
    }
    return result;
}


bool calibration_set_fill(float fine_rps, float coarse_rps) {
    activity_lock();
    bool filling = running && status.step == CALIBRATION_STEP_FILL;
    if (filling) {
        if (isfinite(fine_rps)) {
            status.fine_rps = fminf(fmaxf(fine_rps, 0.0f), (float) get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR));
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, status.fine_rps);
        }
        if (isfinite(coarse_rps)) {
            status.coarse_rps = fminf(fmaxf(coarse_rps, 0.0f),
                                      (float) get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR));
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, status.coarse_rps);
        }
    }
    activity_unlock();
    return filling;
}


// A number as the settings' text, or "null" for NAN
static void setting_text(char *text, size_t room, float value, float scale) {
    if (isnan(value)) {
        snprintf(text, room, "null");
    }
    else {
        snprintf(text, room, "%.4f", (double) (value * scale));
    }
}


bool calibration_save(activity_by_t by) {
    activity_lock();
    bool at_result = running && status.step == CALIBRATION_STEP_RESULT;
    calibration_status_t found = status;
    activity_unlock();
    if (!at_result) {
        return false;
    }
    char text[24];
    const char *why = NULL;
    if (found.part == CALIBRATION_PART_SCALE) {
        setting_text(text, sizeof(text), found.scale.settle_s, 1000.0f);
        why = settings_write_by("settings/scale", "settle_ms", text, by);
        for (int idx = 0; idx < CALIBRATION_LAG_POINTS && why == NULL; idx += 1) {
            char name[24];
            bool measured = idx < found.scale.lag_count;
            snprintf(name, sizeof(name), "lag_%d_flow_per_s", idx + 1);
            setting_text(text, sizeof(text), measured ? found.scale.lag_flow_per_s[idx] : NAN, 1.0f);
            why = settings_write_by("settings/scale", name, text, by);
            snprintf(name, sizeof(name), "lag_%d_ms", idx + 1);
            setting_text(text, sizeof(text), measured ? found.scale.lag_s[idx] : NAN, 1000.0f);
            why = why != NULL ? why : settings_write_by("settings/scale", name, text, by);
        }
        why = why != NULL ? why : settings_store_by("settings/scale", by);
    }
    else if (found.part == CALIBRATION_PART_POWDER) {
        char path[32];
        snprintf(path, sizeof(path), "powder_profiles/%d", found.profile);
        if (!isnan(found.fine.least_rps)) {
            snprintf(text, sizeof(text), "%.4f", (double) found.fine.least_rps);
            why = settings_write_by(path, "fine_min_speed_rps", text, by);
        }
        if (why == NULL && !isnan(found.coarse.least_rps)) {
            snprintf(text, sizeof(text), "%.4f", (double) found.coarse.least_rps);
            why = settings_write_by(path, "coarse_min_speed_rps", text, by);
        }
        why = why != NULL ? why : settings_store_by(path, by);
    }
    if (why != NULL) {
        printf("Calibration: not kept, %s\n", why);
        return false;
    }
    calibration_log(EVENT_STORED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, 0.0f);
    activity_lock();
    answer_given = ANSWER_SAVE;
    activity_unlock();
    jobs_wake();
    return true;
}


void calibration_get_status(calibration_status_t *out) {
    activity_lock();
    *out = status;
    if (out->state == NULL) {
        out->state = "idle";
    }
    activity_t asked;
    if (out->state == NULL) {
        out->profile = -1;
    }
    if (!running && activity_start_pending(&asked) && asked == ACTIVITY_CALIBRATION) {
        out->state = "starting";
        out->part = asked_part;
        out->profile = asked_part == CALIBRATION_PART_POWDER ? asked_profile : -1;
    }
    activity_unlock();
    out->weight = scale_get_current_measurement();
}


// The next word of a console line into word (up to room), and where the line goes on after it
static const char *console_word(const char *line, char *word, size_t room) {
    while (*line == ' ') {
        line += 1;
    }
    size_t len = strcspn(line, " ");
    snprintf(word, room, "%.*s", (int) (len < room ? len : room - 1), line);
    return line + len;
}


bool calibration_console(const char *args, char *reply, size_t reply_size) {
    char word[16];
    const char *rest = console_word(args, word, sizeof(word));
    if (word[0] == '\0') {
        calibration_status_t now;
        calibration_get_status(&now);
        const char *part = calibration_part_name(now.part);
        const char *step = calibration_step_name(now.step);
        snprintf(reply, reply_size, "@ok calibrate state=%s part=%s step=%s progress=%d weight=%.3f line=\"%s\" "
                 "failure=\"%s\"", now.state, part != NULL ? part : "none", step != NULL ? step : "none",
                 now.progress, (double) now.weight, now.line, now.failure);
        return true;
    }
    calibration_part_t part = calibration_part_from_name(word);
    if (part != CALIBRATION_PART_NONE) {
        char given[8];
        console_word(rest, given, sizeof(given));
        int profile = given[0] != '\0' ? atoi(given) : -1;
        calibration_start_result_t started = calibration_request_start(part, profile, ACTIVITY_BY_CONSOLE);
        snprintf(reply, reply_size, started == CALIBRATION_START_OK ? "@ok calibrate %s" :
                 (started == CALIBRATION_START_DRIVER_FAULT ? "@err driver_fault calibrate %s" :
                  "@err busy calibrate %s"), word);
        return started == CALIBRATION_START_OK;
    }
    if (strcmp(word, "answer") == 0) {
        char given[16];
        console_word(rest, given, sizeof(given));
        calibration_answer_result_t result = calibration_answer(given, ACTIVITY_BY_CONSOLE);
        snprintf(reply, reply_size, result == CALIBRATION_ANSWER_OK ? "@ok calibrate answer %s" :
                 (result == CALIBRATION_ANSWER_NO_JOB ? "@err no_job calibrate answer %s" :
                  "@err not_now calibrate answer %s"), given);
        return result == CALIBRATION_ANSWER_OK;
    }
    if (strcmp(word, "fill") == 0) {
        char *end;
        float fine = strtof(rest, &end);
        float coarse = end != rest ? strtof(end, NULL) : NAN;
        bool ok = end != rest && calibration_set_fill(fine, coarse);
        snprintf(reply, reply_size, ok ? "@ok calibrate fill" : "@err not_filling calibrate fill");
        return ok;
    }
    if (strcmp(word, "save") == 0) {
        bool ok = calibration_save(ACTIVITY_BY_CONSOLE);
        snprintf(reply, reply_size, ok ? "@ok calibrate save" : "@err not_now calibrate save");
        return ok;
    }
    if (strcmp(word, "stop") == 0) {
        bool stopped = activity_request_stop(ACTIVITY_CALIBRATION, ACTIVITY_BY_CONSOLE);
        snprintf(reply, reply_size, "@ok calibrate stop stopped=%d", stopped ? 1 : 0);
        return true;
    }
    snprintf(reply, reply_size, "@err usage calibrate [cup|scale|powder|answer <word>|fill <fine> <coarse>|save|stop]");
    return false;
}
