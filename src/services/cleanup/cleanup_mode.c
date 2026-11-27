#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "activity.h"
#include "events.h"
#include "motors.h"
#include "scale.h"
#include "common.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "gate.h"
#include "jobs.h"

// How often a running cleanup looks at the motor drivers when no button is pressed (ADR-012, step 6)
#define CLEANUP_MODE_FAULT_CHECK_MS 100


// Internal
cleanup_mode_config_t cleanup_mode_config;


static char title_string[30];

// True only while cleanup mode's own loop runs, its motors enabled (ADR-013). cleanup_mode_state cannot tell it: the
// web writes ENTER there as soon as it asks for the mode, before the mode has started.
static volatile bool cleanup_mode_running = false;


/*
    The speed cleanup mode asks for stays one the motors will turn at: a number, within the lower of their two limits
    (ADR-012; audit M4). Before, each click added 1 rev/s with no end, and the web could send anything.
*/
static float cleanup_mode_limit_speed(float speed, float fallback) {
    if (!isfinite(speed)) {
        return fallback;
    }

    float max_speed = fminf(get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR),
                            get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR));
    if (speed > max_speed) {
        return max_speed;
    }
    if (speed < -max_speed) {
        return -max_speed;
    }
    return speed;
}


/*
    The cleanup as API v1 reads and drives it (ADR-003, gate 4): a speed with a time runs no longer than that time,
    so a client that loses the network leaves no motor turning; in `fault` until a press or a stop acknowledges it.
*/
static volatile bool cleanup_in_fault = false;
static volatile bool cleanup_has_deadline = false;
static volatile TickType_t cleanup_deadline_tick = 0;


static void cleanup_mode_log(event_kind_t kind, activity_by_t by, event_reason_t reason) {
    event_t event = {.source = EVENT_SOURCE_CLEANUP, .kind = (uint8_t) kind, .by = (uint8_t) by,
                     .reason = (uint8_t) reason};
    events_add(&event);
}


// The cleanup's state in its own words, as the screen shows it (ADR-010, gate 4)
const char *cleanup_mode_get_title(void) {
    return title_string;
}


uint8_t cleanup_mode_menu() {
    // No cleanup with a motor driver not OK, from the menu or the web (ADR-012): the fault screen, before this mode
    // draws anything (step 6). motors_ready() sets up again each driver not OK, so a passing fault does not refuse
    if (!motors_ready()) {
        printf("Cleanup: not started, %s\n", motors_get_fault());
        cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_EXIT;
        return JOBS_EXIT_MOTOR_FAULT;   // the screen shows the fault (ADR-010, gate 2: a job draws nothing of it)
    }

    // Initialize the cleanup mode config
    memset(&cleanup_mode_config, 0x0, sizeof(cleanup_mode_config));
    cleanup_in_fault = false;
    cleanup_has_deadline = false;

    // Enter the clean up mode
    cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_ENTER;

    // The cleanup is the unit's job until it ends (ADR-003, gate 3): the motors' settings are refused meanwhile. Its
    // start is logged with who asked (gate 4)
    activity_job_begin(ACTIVITY_CLEANUP);
    cleanup_mode_log(EVENT_STARTED, activity_job_by(), EVENT_REASON_NONE);
    activity_by_t end_by = ACTIVITY_BY_BUTTON;
    event_reason_t end_reason = EVENT_REASON_STOPPED;
    bool stop_logged = false;

    // Enable both motors
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);
    cleanup_mode_running = true;

    // Open servo gate (if enabled)
    if (gate_state() != GATE_DISABLED) {
        gate_set_ratio(GATE_RATIO_OPEN, true);
    }

    // The title says a fault's reason, and only then: none yet
    title_string[0] = '\0';

    bool quit = false;
    while (!quit) {
        // The motor drivers, a stop and the time's end looked at this often. The speed is set from outside, by the
        // screen or the API (cleanup_mode_set_speed()): the cleanup reads no button (ADR-010, gate 2)
        vTaskDelay(pdMS_TO_TICKS(CLEANUP_MODE_FAULT_CHECK_MS));

        // A motor driver not OK stops both motors and ends the cleanup, naming it (ADR-012, step 6); stopped at the
        // fault, by the unit, and in `fault` until a press or a stop acknowledges it (gate 4)
        const char * motor_fault = motors_get_fault();
        if (motor_fault != NULL) {
            // From here no speed is taken: the motors stay at 0 until the cleanup is acknowledged and ends. Before,
            // a speed sent by the web in `fault` turned both motors again (review D1, 2026-11-16)
            activity_lock();
            cleanup_mode_running = false;
            cleanup_has_deadline = false;
            cleanup_mode_config.trickler_speed = 0;
            motor_set_speed(SELECT_BOTH_MOTOR, 0);
            activity_unlock();
            snprintf(title_string, sizeof(title_string), "%s", motor_fault);
            printf("Cleanup: ended, %s\n", motor_fault);
            cleanup_in_fault = true;
            cleanup_mode_log(EVENT_STOPPED, ACTIVITY_BY_UNIT, EVENT_REASON_FAULT);
            stop_logged = true;

            // Until acknowledged: a stop, which the screen sends for any press at the unit
            while (!activity_take_stop(NULL)) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            break;
        }

        // A stop asked at the unit (by button), by the API or by the console
        activity_by_t stop_by;
        bool stop = false;
        if (activity_take_stop(&stop_by)) {
            stop = true;
            end_by = stop_by;
        }
        // A speed given with a time runs out: the cleanup stops by itself
        else if (cleanup_has_deadline && (int32_t) (xTaskGetTickCount() - cleanup_deadline_tick) >= 0) {
            stop = true;
            end_by = ACTIVITY_BY_UNIT;
            end_reason = EVENT_REASON_TIME;
        }

        if (stop) {
            cleanup_mode_config.trickler_speed = 0;
            motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
            quit = true;
        }
    }

    activity_lock();
    cleanup_mode_running = false;
    cleanup_has_deadline = false;
    activity_unlock();
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);
    if (!stop_logged) {
        cleanup_mode_log(EVENT_STOPPED, end_by, end_reason);
    }
    cleanup_in_fault = false;
    activity_job_end();

    cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_EXIT;

    return 1;  // Return backs to the main menu view
}


cleanup_start_result_t cleanup_mode_request_start(activity_by_t by) {
    // A job, a start asked already, or a person at the unit in a mode or a view of the menu's: not taken over. In
    // the charge's order: busy, then a driver's fault
    activity_t asked_before;
    if (activity_get() != ACTIVITY_NONE || activity_start_pending(&asked_before) ||
        !activity_screen_takes_starts()) {
        return CLEANUP_START_BUSY;
    }
    // Looked at afresh, as at the unit (charge_mode_request_start)
    if (!motors_ready()) {
        return CLEANUP_START_DRIVER_FAULT;
    }
    if (!activity_request_start(ACTIVITY_CLEANUP, by)) {
        return CLEANUP_START_BUSY;
    }
    activity_wake_screen();
    return CLEANUP_START_OK;
}


bool cleanup_mode_set_speed(float speed_rps, float time_s) {
    activity_lock();
    bool running = cleanup_mode_running;
    if (running) {
        cleanup_mode_config.trickler_speed = cleanup_mode_limit_speed(speed_rps, cleanup_mode_config.trickler_speed);
        // The tick before the flag: the cleanup's loop reads them on the other core, without the lock
        if (isfinite(time_s)) {
            cleanup_deadline_tick = xTaskGetTickCount() + pdMS_TO_TICKS((uint32_t) (time_s * 1000.0f));
        }
        cleanup_has_deadline = isfinite(time_s);
        motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
    }
    activity_unlock();
    return running;
}


void cleanup_mode_get_status(cleanup_status_t * status) {
    activity_t asked;
    bool running = activity_get() == ACTIVITY_CLEANUP;
    if (running && cleanup_in_fault) {
        status->state = "fault";
    }
    else if (running) {
        status->state = "running";
    }
    else if (activity_start_pending(&asked) && asked == ACTIVITY_CLEANUP) {
        status->state = "starting";
    }
    else {
        status->state = "idle";
    }
    status->by = running ? activity_by_name(activity_job_by()) : NULL;
    status->speed_rps = running ? cleanup_mode_config.trickler_speed : 0.0f;
    status->time_left_s = NAN;
    if (running && cleanup_has_deadline) {
        int32_t left_ticks = (int32_t) (cleanup_deadline_tick - xTaskGetTickCount());
        status->time_left_s = left_ticks > 0 ? (float) (left_ticks * portTICK_PERIOD_MS) / 1000.0f : 0.0f;
    }
}
