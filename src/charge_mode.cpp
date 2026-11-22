#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include <math.h>

#include "activity.h"
#include "app.h"
#include "events.h"
#include "FloatRingBuffer.h"
#include "mini_12864_module.h"
#include "display.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "eeprom.h"
#include "neopixel_led.h"
#include "profile.h"
#include "common.h"
#include "servo_gate.h"
#include "menu.h"
#include "jobs.h"
#include "charge_control.h"
#include "charge_calibration.h"
#include "charge_pid_learnt.h"


uint8_t charge_weight_digits[] = {0, 0, 0, 0, 0};

charge_mode_config_t charge_mode_config;

// Scale related
extern scale_config_t scale_config;


const eeprom_charge_mode_data_t default_charge_mode_data = {
    .charge_mode_data_rev = 0,

    .coarse_stop_threshold = 5,
    .fine_stop_threshold = 0.03,

    .set_point_sd_margin = 0.02,
    .set_point_mean_margin = 0.02,
    .coarse_stop_gate_ratio = 0,   // NEW
    .coarse_stop_backoff_enable = true,
    .coarse_stop_backoff_turns = 0.25f,
    .coarse_stop_backoff_speed_rps = 1.0f,
    .decimal_places = DP_2,

    // Precharges
    .precharge_enable = false,
    .precharge_time_ms = 1000,
    .precharge_speed_rps = 2,

    // LED related
    .neopixel_normal_charge_colour = RGB_COLOUR_GREEN,        // green
    .neopixel_under_charge_colour = RGB_COLOUR_YELLOW,        // yellow
    .neopixel_over_charge_colour = RGB_COLOUR_RED,            // red
    .neopixel_not_ready_colour = RGB_COLOUR_BLUE,             // blue
};

// Configures
static char title_string[30];

static TickType_t charge_start_tick = 0;
static float last_charge_elapsed_seconds = 0.0f;
static volatile bool charge_timer_running = false;   // the live timer runs only while a throw does (ADR-013)
// The charge ended on a fault, of the scale or of a motor driver, in whatever state it was: its reason takes the
// title line, with no time beside it. Set where it ends, cleared when a charge starts (ADR-013, ADR-012 step 6)
static volatile bool charge_ended_on_fault = false;

/*
    The charge as API v1 reads it (ADR-003, gate 4): the throw's number, from 1 at each start of the unit; whether it
    is paused for readings (ADR-013); the last throw's outcome; and how the charge ended, for its event. Written by
    the charge's task alone.
*/
static volatile uint16_t charge_throw_number = 0;
static volatile bool charge_paused = false;
static volatile uint16_t charge_last_throw = 0;
static volatile event_result_t charge_last_result = EVENT_RESULT_NONE;
static volatile event_reason_t charge_last_reason = EVENT_REASON_NONE;
static volatile float charge_last_weight = NAN;
static activity_by_t charge_end_by = ACTIVITY_BY_BUTTON;
static bool charge_stop_logged = false;     // the charge's stopped event is out: at its fault, or at its end


/*
    The staged controller (ADR-009): one for the whole charge, so what it learns from a throw goes on to the next;
    its calibration is the powder profile's, read as the charge begins. A series for the bench pours its throws into
    the same cup, each from the weight the cup rests at (charge_mode_set_series)
*/
typedef struct {
    charge_control_t controller;
    charge_control_calibration_t cal;
} charge_staged_block_t;
static charge_staged_block_t *charge_staged_block = NULL;     // while a staged charge runs
static charge_control_t *charge_controller = NULL;


static void charge_mode_staged_free(void) {
    if (charge_staged_block != NULL) {
        vPortFree(charge_staged_block);
    }
    charge_staged_block = NULL;
    charge_controller = NULL;
}
static bool charge_staged = false;          // this charge's throws are the staged controller's
static bool charge_staged_verdict = false;  // the last throw ended at its rest, judged by the controller
static float charge_staged_weight = NAN;    // its weight at rest, from the throw's base
static float charge_staged_threshold = NAN; // over or under from this far off the target, judged at rest
static bool charge_pid_learnt = false;      // this charge's throws are the PID's with learning (an experiment)
static charge_pid_t *charge_pid = NULL;     // while such a charge runs, from the heap
static volatile uint16_t charge_series_asked = 0;
static uint16_t charge_series_left = 0;     // this charge's throws still to pour, in a series; 0: no series
static float charge_base = 0.0f;            // the weight a throw starts from: 0, but in a series
// The charge's throws' first errors, each at the calibration's own stop room, carried into the powder profile's
// calibration at the charge's end (ADR-009, 2026-11-22): from throws that came to rest as they should
#define CHARGE_CARRY_THROWS_MAX 32
#define CHARGE_CARRY_LEAST_THROWS 5
static float charge_carry_errors[CHARGE_CARRY_THROWS_MAX];
static uint8_t charge_carry_count = 0;

/*
    The last staged throw's rows, a reading each, for the bench (`ccrec`). 16 bytes a row; at 21 readings a second
    the A&D fills it in 38 s. The Pico W has no room for it (its RAM was within 5 KB of full): its recordings are
    decided with the API's, in a RAM budget of their own (ADR-009, gate 7)
*/
#if PICO_RP2350
#define CHARGE_RECORD_ROWS 800
#else
#define CHARGE_RECORD_ROWS 1
#endif
typedef struct {
    uint32_t t_ms;
    float reading;
    float forecast;         // the reading and what is on its way
    uint16_t coarse_mrps;   // the speeds asked, thousandths of a rev/s
    uint16_t fine_mrps;
} charge_record_row_t;
static charge_record_row_t charge_record[CHARGE_RECORD_ROWS];
static uint16_t charge_record_rows = 0;
static uint16_t charge_record_throw = 0;


void charge_mode_set_series(uint16_t throws) {
    charge_series_asked = throws;
}


// A stop asked at the unit (the screen's Back, by button), by the API or by the console: each ends the charge (gate 4).
// The charge reads no button: the screen turns a press into a command (ADR-010, gate 2)
static bool charge_mode_stop_asked(void) {
    activity_by_t by;
    if (activity_take_stop(&by)) {
        charge_end_by = by;
        return true;
    }
    return false;
}


// A throw that is under way ends, short of its verdict: its event and its outcome (gate 4)
static void charge_mode_throw_ended(event_reason_t reason, uint8_t motor, const char * fault) {
    if (!activity_in_throw()) {
        // Between throws no throw ends, but a fault ends the charge: `last.reason` says why, `last.result` stays the
        // last throw's verdict (api_v1.md; review C2)
        if (reason != EVENT_REASON_STOPPED) {
            charge_last_reason = reason;
        }
        return;
    }
    charge_paused = false;
    charge_last_throw = charge_throw_number;
    charge_last_result = EVENT_RESULT_NONE;
    charge_last_reason = reason;
    charge_last_weight = scale_get_current_measurement();
    event_t ended = {.source = EVENT_SOURCE_THROW, .kind = EVENT_ENDED, .reason = (uint8_t) reason,
                     .throw_number = charge_throw_number, .number = motor, .text = fault};
    events_add(&ended);
    activity_throw_end();
}


// The charge's end, logged once: at a fault, by the unit; else as it was stopped
static void charge_mode_log_stopped(activity_by_t by, event_reason_t reason) {
    if (charge_stop_logged) {
        return;
    }
    charge_stop_logged = true;
    event_t stopped = {.source = EVENT_SOURCE_CHARGE, .kind = EVENT_STOPPED, .by = (uint8_t) by,
                       .reason = (uint8_t) reason};
    events_add(&stopped);
}

// Coarse trickler end-of-trickle backoff
#define COARSE_STOP_BACKOFF_MAX_MS 15000  // safety cap

static bool coarse_backoff_in_progress = false;
static TickType_t coarse_backoff_end_tick = 0;

static void coarse_trickler_backoff(float min_output_speed_rps) {
    if (!charge_mode_config.eeprom_charge_mode_data.coarse_stop_backoff_enable) {
        return;
    }

    float backoff_turns = charge_mode_config.eeprom_charge_mode_data.coarse_stop_backoff_turns;
    if (backoff_turns <= 0.0f) {
        return;
    }

    float configured_speed = charge_mode_config.eeprom_charge_mode_data.coarse_stop_backoff_speed_rps;
    float backoff_speed = configured_speed;
    if (backoff_speed <= 0.0f) {
        backoff_speed = min_output_speed_rps;
    }

    if (backoff_speed <= 0.0f) {
        return;
    }

    float backoff_time_s = backoff_turns / fabsf(backoff_speed);
    uint32_t backoff_time_ms = (uint32_t)(backoff_time_s * 1000.0f);

    if (backoff_time_ms == 0) {
        return;
    }

    if (backoff_time_ms > COARSE_STOP_BACKOFF_MAX_MS) {
        backoff_time_ms = COARSE_STOP_BACKOFF_MAX_MS;
    }

    motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, -fabsf(backoff_speed));
    coarse_backoff_in_progress = true;
    coarse_backoff_end_tick = xTaskGetTickCount() + pdMS_TO_TICKS(backoff_time_ms);
}


// Change of the error per millisecond, for the D term (PR #113). 0 on a throw's first reading: the last error is
// still 0 there, so the whole error counted as its change, and the coarse trickler was given up to three times what
// its P term asks until the next reading (ADR-002, seen on the unit). 0 when no time has passed since the last
// reading: the plain division gives inf or NaN, which the speed limits turn into the maximum speed.
static float error_derivative(float error, float last_error, float elapse_time_ms, bool first_sample) {
    if (first_sample || elapse_time_ms <= 0.0f) {
        return 0.0f;
    }
    return (error - last_error) / elapse_time_ms;
}


// Menu system
extern QueueHandle_t encoder_event_queue;
extern neopixel_led_config_t neopixel_led_config;


// Definitions
typedef enum {
    CHARGE_MODE_EVENT_NO_EVENT = (1 << 0),
    CHARGE_MODE_EVENT_UNDER_CHARGE = (1 << 1),
    CHARGE_MODE_EVENT_OVER_CHARGE = (1 << 2),
} ChargeModeEventBit_t;


static bool charge_mode_end_on_motor_fault(void);


void charge_mode_wait_for_zero() {
    // Set colour to not ready
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour, 
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour, 
        true
    );
    
    // Wait for 5 measurements and wait for stable
    FloatRingBuffer data_buffer(10);

    // Update current status
    snprintf(title_string, sizeof(title_string), "Waiting for Zero");

    // A series (ADR-009): no zero. The throw pours from the weight the cup rests at, steady as the zero test judges
    // it less its zero; a cup that holds enough ends the series
    if (charge_series_left > 0) {
        snprintf(title_string, sizeof(title_string), "Series: steady");
        while (true) {
            TickType_t last_measurement_tick = xTaskGetTickCount();
            if (charge_mode_stop_asked()) {
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                return;
            }
            if (charge_mode_end_on_motor_fault()) {
                return;
            }
            float current_measurement;
            static float last_measurement = 0.0f;
            if (scale_block_wait_for_next_measurement(300, &current_measurement)) {
                data_buffer.enqueue(current_measurement);
                last_measurement = current_measurement;
            }
            if (data_buffer.getCounter() >= 10 &&
                data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin) {
                // A reading, not the mean of ten: the throw is judged on the scale's own digits. The mean of a
                // flickering rest lay between them and put throws off the grid (10.068 gr; one of 10.02 judged over)
                charge_base = last_measurement;
                if (charge_base + charge_mode_config.target_charge_weight > CHARGE_SERIES_CUP_MOST) {
                    printf("Charge: series ended, the cup holds %.2f\n", charge_base);
                    charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                    return;
                }
                charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_COMPLETE;
                return;
            }
            vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(300));
        }
    }

    // A zero asked, at the unit or over the API, starts the count of steady readings again: none from before it, or
    // from before it lands, makes the zero this throw pours from (ADR-003, gate 4)
    uint32_t zero_count = scale_get_zero_count();
    uint8_t readings_since_zero = 0;

    // Stop condition: 10 stable measurements in 200ms apart (2 seconds minimum)
    while (true) {
        TickType_t last_measurement_tick = xTaskGetTickCount();

        // A stop asked; a zero is asked by the screen itself (scale_ask_zero(), ADR-010, gate 2)
        if (charge_mode_stop_asked()) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }

        if (charge_mode_end_on_motor_fault()) {
            return;
        }

        // Perform measurement (max delay 300 seconds   )
        float current_measurement;
        if (scale_block_wait_for_next_measurement(300, &current_measurement)){
            data_buffer.enqueue(current_measurement);
            if (readings_since_zero < UINT8_MAX) {
                readings_since_zero += 1;
            }
        }
        if (scale_get_zero_count() != zero_count) {
            zero_count = scale_get_zero_count();
            readings_since_zero = 0;
        }

        // Generate stop condition
        if (data_buffer.getCounter() >= 10 && readings_since_zero >= 10) {
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin &&
                abs(data_buffer.getMean()) < charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin) {
                // Into pouring under the lock the API's zero takes, and only with no zero asked since: a zero
                // asked over the API is refused from here on (charging)
                activity_lock();
                bool zero_since = scale_get_zero_count() != zero_count;
                if (!zero_since) {
                    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_COMPLETE;
                }
                activity_unlock();
                if (!zero_since) {
                    return;
                }
                zero_count = scale_get_zero_count();
                readings_since_zero = 0;
            }
        }

        // Wait for minimum 300 ms (but can skip if previously wait already)
        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(300));
    }
}

// A missing reading during a charge (ADR-013), judged by the scale's own rhythm, so a slow scale is not a failed one
#define CHARGE_STALE_INTERVALS 3                // no reading for this many usual intervals: the tricklers pause
#define CHARGE_LOST_INTERVALS 10                // this many, and
#define CHARGE_LOST_MIN_MS 1000                 // never under this: the scale is lost
#define CHARGE_USUAL_INTERVAL_UNKNOWN_MS 250    // the rhythm not known yet: take a slow scale's, a G&G's
#define CHARGE_PAUSES_BORNE 2                   // a third pause in one throw ends it
#define CHARGE_STEADY_READINGS 10               // steady after a pause: the zero test less its zero
#define CHARGE_STEADY_SPACING_MS 300
#define CHARGE_STEADY_LIMIT_MS (2 * CHARGE_STEADY_READINGS * CHARGE_STEADY_SPACING_MS)  // not steady by then: unsteady

typedef enum {
    CHARGE_PAUSE_RESUMED = 0,
    CHARGE_PAUSE_EXIT,          // the reset button, or the console's charge stop
    CHARGE_PAUSE_LOST,
    CHARGE_PAUSE_UNSTEADY,
    CHARGE_PAUSE_UNRELIABLE,
} charge_pause_result_t;


/*
    A pause (ADR-013): both tricklers to 0, then wait until readings are back and steady, as the zero test judges
    them less its zero: ten readings 300 ms apart, their spread under set_point_sd_margin. The settled weight is
    given back for the charge to decide from. A pause cannot take back powder already poured, so the charge never
    goes on from the first reading back.
*/
static charge_pause_result_t charge_mode_pause(uint8_t pause_count, uint32_t lost_ms, TickType_t * last_reading_tick,
                                               float * settled_weight) {
    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
    motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

    if (pause_count > CHARGE_PAUSES_BORNE) {
        printf("Charge: pause %u, more than %u in one throw\n", pause_count, CHARGE_PAUSES_BORNE);
        return CHARGE_PAUSE_UNRELIABLE;
    }
    printf("Charge: paused, no reading for %lu ms (pause %u)\n",
           (unsigned long) ((xTaskGetTickCount() - *last_reading_tick) * portTICK_PERIOD_MS), pause_count);
    snprintf(title_string, sizeof(title_string), "Paused: no reading");
    charge_paused = true;
    event_t paused = {.source = EVENT_SOURCE_THROW, .kind = EVENT_PAUSED, .throw_number = charge_throw_number,
                      .number = pause_count};
    events_add(&paused);

    FloatRingBuffer steady_buffer(CHARGE_STEADY_READINGS);
    bool readings_back = false;
    TickType_t back_tick = 0;

    while (true) {
        TickType_t loop_tick = xTaskGetTickCount();

        if (charge_mode_stop_asked()) {
            return CHARGE_PAUSE_EXIT;
        }

        float weight;
        if (scale_block_wait_for_next_measurement(CHARGE_STEADY_SPACING_MS, &weight)) {
            *last_reading_tick = xTaskGetTickCount();
            if (!readings_back) {
                readings_back = true;
                back_tick = *last_reading_tick;
            }
            steady_buffer.enqueue(weight);
            if (steady_buffer.getCounter() >= CHARGE_STEADY_READINGS &&
                steady_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin) {
                *settled_weight = steady_buffer.getMean();
                printf("Charge: resumed from a settled %.3f\n", *settled_weight);
                charge_paused = false;
                event_t resumed = {.source = EVENT_SOURCE_THROW, .kind = EVENT_RESUMED,
                                   .throw_number = charge_throw_number, .value = *settled_weight};
                events_add(&resumed);
                return CHARGE_PAUSE_RESUMED;
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - *last_reading_tick) * portTICK_PERIOD_MS >= lost_ms) {
            return CHARGE_PAUSE_LOST;
        }
        if (readings_back && (now - back_tick) * portTICK_PERIOD_MS >= CHARGE_STEADY_LIMIT_MS) {
            return CHARGE_PAUSE_UNSTEADY;
        }

        vTaskDelayUntil(&loop_tick, pdMS_TO_TICKS(CHARGE_STEADY_SPACING_MS));
    }
}


/*
    The scale failed during a charge (ADR-013), or a motor driver did (ADR-012): the tricklers stay stopped, the gate
    closes, the screen says why, and a press of a button, or a stop asked by the API or the console, goes back to the
    menu. Until then the charge is in `fault` (ADR-003, gate 4): the throw that ran ended, and the charge logged
    stopped by the unit, at the fault; its acknowledgment is no event.
*/
static void charge_mode_scale_fault(const char * reason, event_reason_t throw_reason, uint8_t motor,
                                    const char * fault) {
    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
    motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

    if (servo_gate_enabled()) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, true);
    }

    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour,
        true
    );
    // A throw that was running is over: its timer stops, as at a normal end, and the screen shows the time it ran.
    // Outside a throw the screen keeps the time of the last one
    if (charge_timer_running) {
        last_charge_elapsed_seconds = (float)((xTaskGetTickCount() - charge_start_tick) * portTICK_PERIOD_MS) / 1000.0f;
        charge_timer_running = false;
    }
    charge_ended_on_fault = true;
    snprintf(title_string, sizeof(title_string), "%s", reason);
    printf("Charge: ended, %s\n", reason);
    charge_mode_throw_ended(throw_reason, motor, fault);
    charge_mode_log_stopped(ACTIVITY_BY_UNIT, EVENT_REASON_FAULT);

    // Until acknowledged: a stop, which the screen sends for any press at the unit (ADR-010, gate 2)
    while (!activity_take_stop(NULL)) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
}


// A motor driver that reports a fault, or no longer answers, ends the charge in any of its states (ADR-012: in a
// throw since step 4; waiting for zero and for the cup since step 6). True when it ended the charge
static bool charge_mode_end_on_motor_fault(void) {
    const char * motor_fault = motors_get_fault();
    if (motor_fault == NULL) {
        return false;
    }
    // Which driver, and its state in the API's words (ok, absent, short, hot, reset, supply)
    const char * coarse = motor_driver_get_status_string(SELECT_COARSE_TRICKLER_MOTOR);
    bool coarse_faulted = coarse != NULL && strcmp(coarse, "ok") != 0;
    const char * status = coarse_faulted ? coarse : motor_driver_get_status_string(SELECT_FINE_TRICKLER_MOTOR);
    charge_mode_scale_fault(motor_fault, EVENT_REASON_DRIVER_FAULT, coarse_faulted ? 0 : 1, status);
    return true;
}

static uint32_t charge_mode_now_ms(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


static void charge_mode_record(uint32_t t_ms, float reading, float forecast, float coarse_rps, float fine_rps) {
    if (charge_record_rows >= CHARGE_RECORD_ROWS) {
        return;
    }
    charge_record_row_t *row = &charge_record[charge_record_rows++];
    row->t_ms = t_ms;
    row->reading = reading;
    row->forecast = forecast;
    // A back-off is a negative speed: its row shows 0
    row->coarse_mrps = (uint16_t) lroundf(fminf(fmaxf(coarse_rps, 0.0f), 65.0f) * 1000.0f);
    row->fine_mrps = (uint16_t) lroundf(fminf(fmaxf(fine_rps, 0.0f), 65.0f) * 1000.0f);
}


/*
    One throw by the staged controller (ADR-009), from its zero to its rest: every millisecond each tube's turns by the
    steps counted on its pin, the controller's tick, each reading as it comes, and the speeds it asks for; a small move
    is made to its count by the motors' task. A missing reading pauses both tubes as in the PID loop (ADR-013). True at
    the throw's rest; false when it ended otherwise, the charge's state set.
*/
static bool charge_mode_staged_throw(const char *target_weight_string) {
    const float target = charge_mode_config.target_charge_weight;
    charge_control_t *cc = charge_controller;

    uint32_t usual_interval_ms = scale_get_usual_interval_ms();
    if (usual_interval_ms == 0) {
        usual_interval_ms = CHARGE_USUAL_INTERVAL_UNKNOWN_MS;
    }
    const uint32_t stale_ms = CHARGE_STALE_INTERVALS * usual_interval_ms;
    uint32_t lost_ms = CHARGE_LOST_INTERVALS * usual_interval_ms;
    if (lost_ms < CHARGE_LOST_MIN_MS) {
        lost_ms = CHARGE_LOST_MIN_MS;
    }
    uint8_t pause_count = 0;

    const float coarse_steps_turn = motor_get_steps_per_turn(SELECT_COARSE_TRICKLER_MOTOR);
    const float fine_steps_turn = motor_get_steps_per_turn(SELECT_FINE_TRICKLER_MOTOR);
    const uint32_t coarse_base = motor_get_steps_made(SELECT_COARSE_TRICKLER_MOTOR);
    const uint32_t fine_base = motor_get_steps_made(SELECT_FINE_TRICKLER_MOTOR);

    // The constants as they are now: a `ccon` between throws applies from this one
    cc->k = *charge_calibration_constants();
    cc->moves_by_caller = true;
    const float room_at_start = cc->stop_room_gr;      // where this throw stops, before it learns
    uint32_t start_ms = charge_mode_now_ms();
    charge_control_start(cc, target, start_ms);
    charge_record_rows = 0;
    charge_record_throw = charge_throw_number;

    float coarse_sent = -1.0f;
    float fine_sent = -1.0f;
    bool moving = false;
    TickType_t last_reading_tick = xTaskGetTickCount();
    TickType_t wake = xTaskGetTickCount();
    uint32_t fault_check_ms = start_ms;

    while (!cc->done) {
        if (charge_mode_stop_asked()) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return false;
        }
        uint32_t now_ms = charge_mode_now_ms();
        if (now_ms - fault_check_ms >= 100) {
            fault_check_ms = now_ms;
            if (charge_mode_end_on_motor_fault()) {
                return false;
            }
        }

        float weight;
        bool have_reading = scale_take_new_measurement(&weight);
        if (have_reading) {
            last_reading_tick = xTaskGetTickCount();
            weight -= charge_base;
        }
        else if ((xTaskGetTickCount() - last_reading_tick) * portTICK_PERIOD_MS >= stale_ms) {
            // No fresh reading: no turning (ADR-013). A move cut short is over; the weight settled is the next reading
            pause_count += 1;
            charge_pause_result_t pause = charge_mode_pause(pause_count, lost_ms, &last_reading_tick, &weight);
            coarse_sent = 0.0f;
            fine_sent = 0.0f;
            if (moving) {
                moving = false;
                charge_control_moved(cc, charge_mode_now_ms());
            }
            if (pause == CHARGE_PAUSE_EXIT) {
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                return false;
            }
            if (pause != CHARGE_PAUSE_RESUMED) {
                charge_mode_scale_fault(pause == CHARGE_PAUSE_LOST ? "Scale: no reading" :
                                        pause == CHARGE_PAUSE_UNSTEADY ? "Scale: unsteady" : "Scale: unreliable",
                                        pause == CHARGE_PAUSE_LOST ? EVENT_REASON_SCALE_NO_READING :
                                        pause == CHARGE_PAUSE_UNSTEADY ? EVENT_REASON_SCALE_UNSTEADY :
                                        EVENT_REASON_SCALE_UNRELIABLE, 0, NULL);
                return false;
            }
            snprintf(title_string, sizeof(title_string), "Target: %s", target_weight_string);
            weight -= charge_base;
            have_reading = true;
            now_ms = charge_mode_now_ms();
            wake = xTaskGetTickCount();
        }

        // The turns since the start, as the unit counts its steps
        float coarse_turns = (float) (uint32_t) (motor_get_steps_made(SELECT_COARSE_TRICKLER_MOTOR) - coarse_base) /
                             coarse_steps_turn;
        float fine_turns = (float) (uint32_t) (motor_get_steps_made(SELECT_FINE_TRICKLER_MOTOR) - fine_base) /
                           fine_steps_turn;
        charge_control_count(cc, now_ms, coarse_turns, fine_turns);
        charge_control_tick(cc, now_ms);
        if (have_reading) {
            charge_control_reading(cc, now_ms, weight);
            charge_mode_record(now_ms - start_ms, weight, cc->reading + cc->unseen_gr, cc->coarse_rps, cc->fine_rps);
        }

        // A small move, made to its count from a stop; the fine tube's speed otherwise
        if (moving) {
            if (motor_move_done(SELECT_FINE_TRICKLER_MOTOR)) {
                moving = false;
                fine_sent = 0.0f;
                charge_control_moved(cc, charge_mode_now_ms());
            }
        }
        else if (cc->phase == CHARGE_CONTROL_PHASE_MOVE && cc->move_turns >= 0.0f) {
            if (motor_move_turns(SELECT_FINE_TRICKLER_MOTOR, cc->move_turns, cc->k.move_rps)) {
                moving = true;
            }
        }
        else if (cc->fine_rps != fine_sent) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, cc->fine_rps);
            fine_sent = cc->fine_rps;
        }
        if (cc->coarse_rps != coarse_sent) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, cc->coarse_rps);
            coarse_sent = cc->coarse_rps;
        }

        vTaskDelayUntil(&wake, 1);
    }
    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
    motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

    // Its first error, at the calibration's own stop room, for the carry at the charge's end
    if (cc->have_first_error && cc->streams == 0 && cc->result != CHARGE_CONTROL_RESULT_LIMIT &&
        charge_carry_count < CHARGE_CARRY_THROWS_MAX) {
        charge_carry_errors[charge_carry_count++] = cc->first_error_gr + room_at_start - cc->cal.stop_room_gr;
    }

    charge_staged_verdict = true;
    charge_staged_weight = cc->rest_weight;
    charge_staged_threshold = cc->cal.band_gr + 0.5f * cc->cal.digit_gr;
    float elapsed_s = (float) (charge_mode_now_ms() - start_ms) / 1000.0f;
    printf("Charge: throw n=%u result=%s rest=%.3f target=%.3f first=%.3f moves=%u streams=%u elapsed=%.2f "
           "coarse_turns=%.4f fine_turns=%.4f stop_room=%.4f fine_gr=%.4f fine_stream=%.4f learnt=%u base=%.3f "
           "rows=%u\n",
           charge_throw_number, charge_control_result_string((charge_control_result_t) cc->result), cc->rest_weight,
           target, cc->have_first_error ? cc->first_error_gr : NAN, cc->moves, cc->streams, elapsed_s,
           cc->turned[0], cc->turned[1], cc->stop_room_gr, cc->cal.fine_gr_turn, cc->fine_gr_stream, cc->fine_learn_n,
           charge_base, charge_record_rows);
    return true;
}


/*
    One throw by the PID loop with learning (ADR-009, the experiment of 2026-11-22 evening), from its zero to its rest:
    every millisecond the fine tube's turns by the steps counted on its pin and the controller's tick, each reading as
    it comes, and the speeds it asks for (a negative coarse speed is today's back-off). A missing reading pauses both
    tubes as in the other loops (ADR-013). True at the throw's rest; false when it ended otherwise, the charge's state
    set.
*/
static bool charge_mode_pid_learnt_throw(void) {
    const float target = charge_mode_config.target_charge_weight;
    charge_pid_t *c = charge_pid;
    const eeprom_charge_mode_data_t *data = &charge_mode_config.eeprom_charge_mode_data;
    profile_t *profile = profile_get_selected();

    uint32_t usual_interval_ms = scale_get_usual_interval_ms();
    if (usual_interval_ms == 0) {
        usual_interval_ms = CHARGE_USUAL_INTERVAL_UNKNOWN_MS;
    }
    const uint32_t stale_ms = CHARGE_STALE_INTERVALS * usual_interval_ms;
    uint32_t lost_ms = CHARGE_LOST_INTERVALS * usual_interval_ms;
    if (lost_ms < CHARGE_LOST_MIN_MS) {
        lost_ms = CHARGE_LOST_MIN_MS;
    }
    uint8_t pause_count = 0;

    // Today's loop's settings, its speeds within the motors' limits as it takes them
    const charge_pid_constants_t *k = charge_pid_constants();
    charge_pid_settings_t s = {
        .coarse_kp = profile->coarse_kp,
        .coarse_ki = profile->coarse_ki,
        .coarse_kd = profile->coarse_kd,
        .coarse_min_rps = fmaxf(get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR), profile->coarse_min_flow_speed_rps),
        .coarse_max_rps = fminf((float) get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                profile->coarse_max_flow_speed_rps),
        .fine_kp = profile->fine_kp,
        .fine_ki = profile->fine_ki,
        .fine_kd = profile->fine_kd,
        .fine_min_rps = fmaxf(get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR), profile->fine_min_flow_speed_rps),
        .fine_max_rps = fminf((float) get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR), profile->fine_max_flow_speed_rps),
        .coarse_stop_threshold_gr = data->coarse_stop_threshold,
        .backoff_turns = data->coarse_stop_backoff_enable ? data->coarse_stop_backoff_turns : 0.0f,
        .backoff_rps = fabsf(data->coarse_stop_backoff_speed_rps),
        .digit_gr = k->digit_gr,
        .interval_s = (float) usual_interval_ms / 1000.0f,
        .band_gr = data->trickle_mode == CHARGE_TRICKLE_FAST ? CHARGE_TRICKLE_FAST_BAND_GR :
                   CHARGE_TRICKLE_ACCURATE_BAND_GR,
    };
    charge_pid_learning_t *learning = charge_pid_learning((uint8_t) profile_get_selected_idx());
    const float fine_steps_turn = motor_get_steps_per_turn(SELECT_FINE_TRICKLER_MOTOR);
    const uint32_t fine_base = motor_get_steps_made(SELECT_FINE_TRICKLER_MOTOR);

    uint32_t start_ms = charge_mode_now_ms();
    const float gr_turn_at_start = learning->gr_turn > 0.0f ? learning->gr_turn : k->gr_turn_prior;
    charge_pid_start(c, &s, k, learning, target, start_ms);
    charge_record_rows = 0;
    charge_record_throw = charge_throw_number;

    float coarse_sent = NAN;
    float fine_sent = NAN;
    TickType_t last_reading_tick = xTaskGetTickCount();
    TickType_t wake = xTaskGetTickCount();
    uint32_t fault_check_ms = start_ms;

    while (!c->done) {
        if (charge_mode_stop_asked()) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return false;
        }
        uint32_t now_ms = charge_mode_now_ms();
        if (now_ms - fault_check_ms >= 100) {
            fault_check_ms = now_ms;
            if (charge_mode_end_on_motor_fault()) {
                return false;
            }
        }

        float weight;
        bool have_reading = scale_take_new_measurement(&weight);
        if (have_reading) {
            last_reading_tick = xTaskGetTickCount();
            weight -= charge_base;
        }
        else if ((xTaskGetTickCount() - last_reading_tick) * portTICK_PERIOD_MS >= stale_ms) {
            // No fresh reading: no turning (ADR-013)
            pause_count += 1;
            charge_pause_result_t pause = charge_mode_pause(pause_count, lost_ms, &last_reading_tick, &weight);
            coarse_sent = NAN;
            fine_sent = NAN;
            if (pause == CHARGE_PAUSE_EXIT) {
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                return false;
            }
            if (pause != CHARGE_PAUSE_RESUMED) {
                charge_mode_scale_fault(pause == CHARGE_PAUSE_LOST ? "Scale: no reading" :
                                        pause == CHARGE_PAUSE_UNSTEADY ? "Scale: unsteady" : "Scale: unreliable",
                                        pause == CHARGE_PAUSE_LOST ? EVENT_REASON_SCALE_NO_READING :
                                        pause == CHARGE_PAUSE_UNSTEADY ? EVENT_REASON_SCALE_UNSTEADY :
                                        EVENT_REASON_SCALE_UNRELIABLE, 0, NULL);
                return false;
            }
            weight -= charge_base;
            have_reading = true;
            now_ms = charge_mode_now_ms();
            wake = xTaskGetTickCount();
        }

        float fine_turns = (float) (uint32_t) (motor_get_steps_made(SELECT_FINE_TRICKLER_MOTOR) - fine_base) /
                           fine_steps_turn;
        charge_pid_count(c, fine_turns);
        charge_pid_tick(c, now_ms);
        if (have_reading) {
            charge_pid_reading(c, now_ms, weight);
            charge_mode_record(now_ms - start_ms, weight, c->reading + c->unseen_gr, c->coarse_rps, c->fine_rps);
        }
        // NAN after a pause: sent again whatever it is
        if (!(c->fine_rps == fine_sent)) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, c->fine_rps);
            fine_sent = c->fine_rps;
        }
        if (!(c->coarse_rps == coarse_sent)) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, c->coarse_rps);
            coarse_sent = c->coarse_rps;
        }

        vTaskDelayUntil(&wake, 1);
    }
    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
    motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

    charge_staged_verdict = true;
    charge_staged_weight = c->rest_weight;
    charge_staged_threshold = s.band_gr + 0.5f * s.digit_gr;
    float elapsed_s = (float) (charge_mode_now_ms() - start_ms) / 1000.0f;
    // In the staged controller's form, so the bench's series reads it: moves are the top-ups, streams none
    printf("Charge: throw n=%u result=%s rest=%.3f target=%.3f first=%.3f moves=%u streams=0 elapsed=%.2f "
           "fine_turns=%.4f gr_turn=%.4f gr_turn_before=%.4f learnt=%u controller=pid_learnt base=%.3f rows=%u\n",
           charge_throw_number, charge_control_result_string((charge_control_result_t) c->result), c->rest_weight,
           target, c->have_first_error ? c->first_error_gr : NAN, c->tops, elapsed_s, c->fine_turns,
           learning->gr_turn, gr_turn_at_start, learning->learnt, charge_base, charge_record_rows);
    return true;
}

void charge_mode_record_console(char *reply, size_t reply_size, void (*write)(const char *line, void *ctx),
                                void *write_ctx) {
    char line[80];
    for (uint16_t i = 0; i < charge_record_rows; i++) {
        const charge_record_row_t *row = &charge_record[i];
        snprintf(line, sizeof(line), "@r %lu %.3f %.4f %.3f %.3f", (unsigned long) row->t_ms, row->reading,
                 row->forecast, row->coarse_mrps / 1000.0f, row->fine_mrps / 1000.0f);
        write(line, write_ctx);
    }
    snprintf(reply, reply_size, "@ok ccrec throw=%u rows=%u", charge_record_throw, charge_record_rows);
}


void charge_mode_wait_for_complete() {

    charge_start_tick = xTaskGetTickCount();
    charge_timer_running = true;
    coarse_backoff_in_progress = false;

    // Set colour to under charge
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour, 
        charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour, 
        true
    );

    // If the servo gate is used then it has to be opened
    if (servo_gate_enabled()) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_OPEN, false);
    }
    // Update current status
    char target_weight_string[WEIGHT_STRING_LEN];
    float_to_string(target_weight_string, charge_mode_config.target_charge_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);

    snprintf(title_string, sizeof(title_string), 
             "Target: %s", 
             target_weight_string);

    // The staged controller pours the throw, to its rest (ADR-009); the PID loop below otherwise, as it was
    charge_staged_verdict = false;
    if (charge_staged) {
        if (!charge_mode_staged_throw(target_weight_string)) {
            return;
        }
        last_charge_elapsed_seconds = (float)((xTaskGetTickCount() - charge_start_tick) * portTICK_PERIOD_MS) / 1000.0f;
        charge_timer_running = false;
        if (servo_gate_enabled()) {
            servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, true);
        }
        charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_CUP_REMOVAL;
        return;
    }
    if (charge_pid_learnt) {
        if (!charge_mode_pid_learnt_throw()) {
            return;
        }
        last_charge_elapsed_seconds = (float)((xTaskGetTickCount() - charge_start_tick) * portTICK_PERIOD_MS) / 1000.0f;
        charge_timer_running = false;
        if (servo_gate_enabled()) {
            servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, true);
        }
        charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_CUP_REMOVAL;
        return;
    }

    // Read trickling parameter from the current profile
    profile_t * current_profile = profile_get_selected();

    // Find the minimum of max speed from the motor and the profile
    float coarse_trickler_max_speed = fmin(get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           current_profile->coarse_max_flow_speed_rps);
    float coarse_trickler_min_speed = fmax(get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           current_profile->coarse_min_flow_speed_rps);
    float fine_trickler_max_speed = fmin(get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         current_profile->fine_max_flow_speed_rps);
    float fine_trickler_min_speed = fmax(get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         current_profile->fine_min_flow_speed_rps);

    // Define PID terms
    float coarse_trickler_integral = 0.0f;
    float fine_trickler_integral = 0.0f;
    float coarse_trickler_last_error = 0.0f;
    float fine_trickler_last_error = 0.0f;

    // Calculate target weight for coarse trickler
    // The coarse trickler is suppose to stop ahead of the target weight by an offset
    float coarse_trickler_target_charge_weight = fmaxf(0.0f, charge_mode_config.target_charge_weight - charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold);

    TickType_t last_sample_tick = xTaskGetTickCount();
    TickType_t current_sample_tick = last_sample_tick;
    bool should_coarse_trickler_move = true;
    bool first_sample = true;   // no D term before the second reading

    // A missing reading pauses the tricklers, and a lost scale ends the charge, by the scale's own rhythm (ADR-013)
    uint32_t usual_interval_ms = scale_get_usual_interval_ms();
    if (usual_interval_ms == 0) {
        usual_interval_ms = CHARGE_USUAL_INTERVAL_UNKNOWN_MS;
    }
    const uint32_t stale_ms = CHARGE_STALE_INTERVALS * usual_interval_ms;
    uint32_t lost_ms = CHARGE_LOST_INTERVALS * usual_interval_ms;
    if (lost_ms < CHARGE_LOST_MIN_MS) {
        lost_ms = CHARGE_LOST_MIN_MS;
    }
    uint8_t pause_count = 0;
    TickType_t last_reading_tick = xTaskGetTickCount();
    printf("Charge: the scale's usual interval %lu ms; a pause after %lu ms without a reading, lost after %lu ms\n",
           (unsigned long) usual_interval_ms, (unsigned long) stale_ms, (unsigned long) lost_ms);

    while (true) {
        if (charge_mode_stop_asked()) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }

        // Run the PID controlled loop to start charging
        // Perform the measurement: wait no longer than the stale point, and no more than 200 ms between button polls
        float current_weight;
        bool resumed = false;
        uint32_t silent_ms = (xTaskGetTickCount() - last_reading_tick) * portTICK_PERIOD_MS;
        uint32_t wait_ms = silent_ms < stale_ms ? stale_ms - silent_ms : 1;
        if (wait_ms > 200) {
            wait_ms = 200;
        }
        if (scale_block_wait_for_next_measurement(wait_ms, &current_weight)) {
            last_reading_tick = xTaskGetTickCount();
            current_weight -= charge_base;      // 0, but in a bench's series (ADR-009)
        }
        else {
            silent_ms = (xTaskGetTickCount() - last_reading_tick) * portTICK_PERIOD_MS;
            if (silent_ms < stale_ms) {
                continue;
            }

            pause_count += 1;
            charge_pause_result_t pause = charge_mode_pause(pause_count, lost_ms, &last_reading_tick, &current_weight);
            if (pause == CHARGE_PAUSE_EXIT) {
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                return;
            }
            if (pause != CHARGE_PAUSE_RESUMED) {
                charge_mode_scale_fault(pause == CHARGE_PAUSE_LOST ? "Scale: no reading" :
                                        pause == CHARGE_PAUSE_UNSTEADY ? "Scale: unsteady" : "Scale: unreliable",
                                        pause == CHARGE_PAUSE_LOST ? EVENT_REASON_SCALE_NO_READING :
                                        pause == CHARGE_PAUSE_UNSTEADY ? EVENT_REASON_SCALE_UNSTEADY :
                                        EVENT_REASON_SCALE_UNRELIABLE, 0, NULL);
                return;
            }
            snprintf(title_string, sizeof(title_string), "Target: %s", target_weight_string);
            current_weight -= charge_base;
            resumed = true;
        }
        current_sample_tick = xTaskGetTickCount();

        // A motor driver that reports a fault, or no longer answers, ends the throw as a failed scale does (ADR-012)
        if (charge_mode_end_on_motor_fault()) {
            return;
        }

        // Handle coarse trickler backoff stop
        if (coarse_backoff_in_progress && (current_sample_tick >= coarse_backoff_end_tick)) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            coarse_backoff_in_progress = false;
        }

        float coarse_trickler_error = coarse_trickler_target_charge_weight - current_weight;
        float fine_trickler_error = charge_mode_config.target_charge_weight - current_weight;

        // After a pause the D term starts again as on a first reading, so the gap makes no spike; the I term took
        // nothing during it (ADR-013)
        if (resumed) {
            fine_trickler_last_error = fine_trickler_error;
            coarse_trickler_last_error = coarse_trickler_error;
        }

        // Fine & Coarse trickler stop condition
        if (fine_trickler_error < charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold) {
            // Stop all motors
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            break;
        }

        // Coarse trickler stop condition
        else if (coarse_trickler_error <= 0 &&
                 should_coarse_trickler_move) {

            should_coarse_trickler_move = false;
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            // NEW: When the coarse trickler stops, move the servo gate to a configured ratio
            // Ratio convention: 0.0 = open, 1.0 = close
            if (servo_gate_enabled()) {
                float r = charge_mode_config.eeprom_charge_mode_data.coarse_stop_gate_ratio;

                servo_gate_set_ratio(r, false); // don't block the charge loop
            }

            // NEW: Reverse the coarse trickler for a short backoff (quarter turn).
            coarse_trickler_backoff(coarse_trickler_min_speed);
        }
    

        // Update fine trickler speed
        float elapse_time_ms = (current_sample_tick - last_sample_tick) / portTICK_RATE_MS;
        fine_trickler_integral += fine_trickler_error;
        float fine_trickler_derivative = error_derivative(fine_trickler_error, fine_trickler_last_error, elapse_time_ms, first_sample);

        // Update fine trickler speed
        float new_p = current_profile->fine_kp * fine_trickler_error;
        float new_i = current_profile->fine_ki * fine_trickler_integral;
        float new_d = current_profile->fine_kd * fine_trickler_derivative;
        float new_speed = fmaxf(fine_trickler_min_speed, fminf(new_p + new_i + new_d, fine_trickler_max_speed));
        motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, new_speed);

        // Update coarse trickler speed
        if (should_coarse_trickler_move && !coarse_backoff_in_progress) {
            coarse_trickler_integral += coarse_trickler_error;
            float coarse_trickler_derivative = error_derivative(coarse_trickler_error, coarse_trickler_last_error, elapse_time_ms, first_sample);

            new_p = current_profile->coarse_kp * coarse_trickler_error;
            new_i = current_profile->coarse_ki * coarse_trickler_integral;
            new_d = current_profile->coarse_kd * coarse_trickler_derivative ;

            new_speed = fmaxf(coarse_trickler_min_speed, fminf(new_p + new_i + new_d, coarse_trickler_max_speed));

            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, new_speed);
        }

        // Record state
        last_sample_tick = current_sample_tick;
        fine_trickler_last_error = fine_trickler_error;
        coarse_trickler_last_error = coarse_trickler_error;
        first_sample = false;
    }

    // Stop the timer
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed_ticks = now - charge_start_tick;
    last_charge_elapsed_seconds = (float)(elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;
    charge_timer_running = false;

    // Close the gate if the servo gate is present
    if (servo_gate_enabled()) {
    servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, true);
    }

    // Precharge
    if (charge_mode_config.eeprom_charge_mode_data.precharge_enable &&
    servo_gate_enabled()) {
        // Set a fixed delay between closing the gate and precharge to allow the gate to fully close
        vTaskDelay(pdMS_TO_TICKS(500));

        // Start the pre-charge
        motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, charge_mode_config.eeprom_charge_mode_data.precharge_speed_rps);
        vTaskDelay(pdMS_TO_TICKS(charge_mode_config.eeprom_charge_mode_data.precharge_time_ms));

        motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
    }
    else {
        vTaskDelay(pdMS_TO_TICKS(20));  // Wait for other tasks to complete  
    }

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_CUP_REMOVAL;
}

void charge_mode_wait_for_cup_removal() {
    // Update current status
    snprintf(title_string, sizeof(title_string), "Remove Cup");

    FloatRingBuffer data_buffer(5);

    // Post charge analysis (while waiting for removal of the cup)
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for other tasks to complete

    // Take current measurement: the PID loop's a second after its stop; the staged controller's at its rest, judged
    // by its band, half a digit beyond it (0.03 gr for 0.02 gr on the A&D, today's threshold) (ADR-009)
    float current_measurement = scale_get_current_measurement() - charge_base;
    float fine_stop_threshold = charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;
    if (charge_staged_verdict) {
        current_measurement = charge_staged_weight;
        fine_stop_threshold = charge_staged_threshold;
    }
    float error = charge_mode_config.target_charge_weight - current_measurement;

    // Update LED colour before moving to the next stage
    // Over charged
    if (error <= -fine_stop_threshold) {
        neopixel_led_set_colour(
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
            charge_mode_config.eeprom_charge_mode_data.neopixel_over_charge_colour, 
            charge_mode_config.eeprom_charge_mode_data.neopixel_over_charge_colour, 
            true
        );

        // Set over charge
        charge_mode_config.charge_mode_event |= CHARGE_MODE_EVENT_OVER_CHARGE;
    }
    // Under charged
    else if (error >= fine_stop_threshold) {
        neopixel_led_set_colour(
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour, 
            charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour, 
            charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour, 
            true
        );

        // Set under charge flag
        charge_mode_config.charge_mode_event |= CHARGE_MODE_EVENT_UNDER_CHARGE;

    }
    // Normal
    else {
        neopixel_led_set_colour(
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour, 
            charge_mode_config.eeprom_charge_mode_data.neopixel_normal_charge_colour, 
            charge_mode_config.eeprom_charge_mode_data.neopixel_normal_charge_colour, 
            true
        );

        // Clear over and under charge bit
        charge_mode_config.charge_mode_event &= ~(CHARGE_MODE_EVENT_UNDER_CHARGE | CHARGE_MODE_EVENT_OVER_CHARGE);
    }

    // The verdict is given, by the test above: the throw is finished, and what it read may be written again
    // (ADR-003, gates 3 and 4)
    event_result_t result = error <= -fine_stop_threshold ? EVENT_RESULT_OVER :
                            error >= fine_stop_threshold ? EVENT_RESULT_UNDER : EVENT_RESULT_GOOD;
    charge_last_throw = charge_throw_number;
    charge_last_result = result;
    charge_last_reason = EVENT_REASON_FINISHED;
    charge_last_weight = current_measurement;
    event_t finished = {.source = EVENT_SOURCE_THROW, .kind = EVENT_FINISHED, .throw_number = charge_throw_number,
                        .result = (uint8_t) result, .value = current_measurement,
                        .value2 = last_charge_elapsed_seconds};
    events_add(&finished);
    activity_throw_end();

    // A series goes on to its next throw in the same cup, with no cup lifted (ADR-009)
    if (charge_series_left > 0) {
        charge_series_left -= 1;
        charge_mode_config.charge_mode_state = charge_series_left > 0 ? CHARGE_MODE_WAIT_FOR_ZERO : CHARGE_MODE_EXIT;
        return;
    }

    // Stop condition: 5 stable measurements in 300ms apart (1.5 seconds minimum)
    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        if (charge_mode_stop_asked()) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }

        if (charge_mode_end_on_motor_fault()) {
            return;
        }

        // Perform measurement
        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            // If no measurement within 200ms then poll the button and retry
            continue;
        }
        data_buffer.enqueue(current_weight);

        // Generate stop condition
        if (data_buffer.getCounter() >= 5) {
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin && 
                data_buffer.getMean() + 10 < charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin){
                break;
            }
        }

        // Wait for next measurement
        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(300));
    }

    // Reset LED to default colour
    neopixel_led_set_colour(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour,
                            true);

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_CUP_RETURN;
}

void charge_mode_wait_for_cup_return() { 
    // Set colour to not ready
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour, 
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour, 
        charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour, 
        true
    );

    snprintf(title_string, sizeof(title_string), "Return Cup");


    FloatRingBuffer data_buffer(5);

    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        // A stop asked; a zero is asked by the screen itself (scale_ask_zero(), ADR-010, gate 2)
        if (charge_mode_stop_asked()) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }

        if (charge_mode_end_on_motor_fault()) {
            return;
        }

        // Perform measurement
        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            // If no measurement within 200ms then poll the button and retry
            continue;
        }

        if (current_weight >= 0) {
            break;
        }

        // Wait for next measurement
        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(20));
    }

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_ZERO;
}


// The charge's state in its own words, as the screen shows it (ADR-010, gate 4: the screen draws, the charge does not)
const char *charge_mode_get_title(void) {
    return title_string;
}


uint8_t charge_mode_menu(void) {
    // No charge with a motor driver not OK, from the menu or the web (ADR-012): the fault screen, before this mode
    // draws anything (step 6). motors_ready() sets up again each driver not OK, so a passing fault does not refuse
    if (!motors_ready()) {
        printf("Charge: not started, %s\n", motors_get_fault());
        charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
        return JOBS_EXIT_MOTOR_FAULT;   // the screen shows the fault (ADR-010, gate 2: a job draws nothing of it)
    }

    // A new charge: no fault of the last one on its screen
    charge_ended_on_fault = false;

    // The charge is the unit's job until it ends (ADR-003, gate 3): the motors' settings are refused meanwhile. Its
    // start is logged with who asked (gate 4)
    // One target for the unit (ADR-003, gate 4): the menu's weight digits, which the API sets too. Copied as the job
    // begins, under the lock the API writes the target under (review D2)
    activity_lock();
    charge_mode_config.target_charge_weight = charge_mode_get_target();
    activity_job_begin(ACTIVITY_CHARGE);
    activity_unlock();
    charge_end_by = ACTIVITY_BY_BUTTON;
    charge_stop_logged = false;
    event_t started = {.source = EVENT_SOURCE_CHARGE, .kind = EVENT_STARTED, .by = (uint8_t) activity_job_by(),
                       .number = (uint8_t) profile_get_selected_idx(),
                       .value = charge_mode_config.target_charge_weight};
    events_add(&started);

    // The controller for the whole charge, and a series if one was asked (ADR-009)
    charge_series_left = charge_series_asked;
    charge_series_asked = 0;
    charge_base = 0.0f;
    charge_carry_count = 0;
    charge_staged = charge_mode_config.eeprom_charge_mode_data.controller == CHARGE_CONTROLLER_STAGED;
    charge_pid_learnt = charge_mode_config.eeprom_charge_mode_data.controller == CHARGE_CONTROLLER_PID_LEARNT;
    if (charge_pid_learnt) {
        charge_pid = (charge_pid_t *) pvPortMalloc(sizeof(charge_pid_t));
        if (charge_pid == NULL) {
            charge_pid_learnt = false;
            charge_mode_scale_fault("No memory", EVENT_REASON_REFUSED, 0, NULL);
            activity_job_end();
            return 1;
        }
        const charge_pid_learning_t *l = charge_pid_learning((uint8_t) profile_get_selected_idx());
        printf("Charge: pid_learnt, profile %u, figure %.4f gr a turn from %u throws\n", profile_get_selected_idx(),
               l->gr_turn, l->learnt);
    }
    if (charge_staged) {
        // The controller and its calibration for this charge, from the heap, given back as the charge ends: the Pico
        // W has no RAM to keep them for a charge that may never come
        charge_staged_block = (charge_staged_block_t *) pvPortMalloc(sizeof(charge_staged_block_t));
        // The mode's band: the calibration's rules are made for it at each charge's start (ADR-009, 2026-11-22)
        float band_gr = charge_mode_config.eeprom_charge_mode_data.trickle_mode == CHARGE_TRICKLE_FAST ?
                        CHARGE_TRICKLE_FAST_BAND_GR : CHARGE_TRICKLE_ACCURATE_BAND_GR;
        const char *refused = charge_staged_block == NULL ? "No memory" :
            !charge_calibration_get((uint8_t) profile_get_selected_idx(), band_gr, &charge_staged_block->cal) ?
            "Not calibrated" : NULL;
        if (refused != NULL) {
            // Refused at the start already (charge_mode_request_start) when not calibrated; here only if the slot
            // changed since
            charge_mode_staged_free();
            charge_mode_scale_fault(refused, EVENT_REASON_REFUSED, 0, NULL);
            activity_job_end();
            return 1;
        }
        charge_controller = &charge_staged_block->controller;
        charge_control_init(charge_controller, &charge_staged_block->cal, charge_calibration_constants(), 0.0f, false);
        printf("Charge: staged, profile %u, %s at %.3f, stop room %.4f\n", profile_get_selected_idx(),
               charge_controller->cal.finish == CHARGE_CONTROL_FINISH_JOG ? "jog" : "glide",
               charge_controller->cal.band_gr, charge_controller->cal.stop_room_gr);
    }

    // Enable motor on entering the charge mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_ZERO;

    bool quit = false;
    while (quit == false) {
        switch (charge_mode_config.charge_mode_state) {
            case CHARGE_MODE_WAIT_FOR_ZERO: {
                // A throw from its zeroing to its verdict: what it reads is not written meanwhile (gate 3). Each is
                // numbered and logged (gate 4)
                activity_throw_begin();
                charge_throw_number += 1;
                charge_paused = false;
                event_t throw_started = {.source = EVENT_SOURCE_THROW, .kind = EVENT_STARTED,
                                         .throw_number = charge_throw_number,
                                         .number = (uint8_t) profile_get_selected_idx(),
                                         .value = charge_mode_config.target_charge_weight};
                events_add(&throw_started);
                charge_mode_wait_for_zero();
                break;
            }
            case CHARGE_MODE_WAIT_FOR_COMPLETE:
                charge_mode_wait_for_complete();
                break;
            case CHARGE_MODE_WAIT_FOR_CUP_REMOVAL:
                charge_mode_wait_for_cup_removal();
                break;
            case CHARGE_MODE_WAIT_FOR_CUP_RETURN:
                charge_mode_wait_for_cup_return();
                break;
            case CHARGE_MODE_EXIT:
            default:
                quit = true;
                break;
        }
    }

    // Reset LED to default colour
    neopixel_led_set_colour(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour,
                            true);

    // Diable motors on exiting the mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);

    // The job is over, a throw cut short with it (ADR-003, gates 3 and 4): logged, unless a fault logged it
    charge_mode_throw_ended(EVENT_REASON_STOPPED, 0, NULL);
    charge_mode_log_stopped(charge_end_by, EVENT_REASON_STOPPED);
    charge_staged_verdict = false;
    if (charge_staged && charge_carry_count >= CHARGE_CARRY_LEAST_THROWS) {
        float before = NAN, after = NAN;
        uint8_t profile = (uint8_t) profile_get_selected_idx();
        if (charge_calibration_carry(profile, charge_carry_errors, charge_carry_count, &before, &after)) {
            printf("Charge: carried %u first errors into profile %u's calibration, its stop placed from a median of "
                   "%.4f, was %.4f\n", charge_carry_count, profile, after, before);
        }
        else {
            printf("Charge: %u first errors not carried (from %.4f to %.4f, or no calibration stored)\n",
                   charge_carry_count, before, after);
        }
    }
    charge_carry_count = 0;
    charge_mode_staged_free();
    if (charge_pid != NULL) {
        vPortFree(charge_pid);
        charge_pid = NULL;
    }
    charge_pid_learnt = false;
    activity_job_end();

    return 1;  // return back to main menu
}


bool charge_mode_config_init(void) {
    bool is_ok = false;

    // Read charge mode config from EEPROM
    is_ok = load_config(EEPROM_CHARGE_MODE_BASE_ADDR, &charge_mode_config.eeprom_charge_mode_data, &default_charge_mode_data, sizeof(charge_mode_config.eeprom_charge_mode_data), EEPROM_CHARGE_MODE_DATA_REV);
    if (!is_ok) {
        printf("Unable to read charge mode configuration\n");
        return is_ok;
    }

    // The controller sits in a filler byte (ADR-009): anything but a known loop there is the staged controller
    if (charge_mode_config.eeprom_charge_mode_data.controller > CHARGE_CONTROLLER_PID_LEARNT) {
        charge_mode_config.eeprom_charge_mode_data.controller = CHARGE_CONTROLLER_STAGED;
    }
    if (charge_mode_config.eeprom_charge_mode_data.trickle_mode > CHARGE_TRICKLE_FAST) {
        charge_mode_config.eeprom_charge_mode_data.trickle_mode = CHARGE_TRICKLE_ACCURATE;
    }

    // Register to eeprom save all
    eeprom_register_handler(charge_mode_config_save);
    charge_mode_settings_register();

    return true;
}


bool charge_mode_config_save(void) {
    bool is_ok = save_config(EEPROM_CHARGE_MODE_BASE_ADDR, &charge_mode_config.eeprom_charge_mode_data, sizeof(eeprom_charge_mode_data_t));
    return is_ok;
}


/*
    One target for the unit (ADR-003, gate 4): the menu's five weight digits, two or three of them decimals, show it
    and set it, and the API reads and writes the same. 0 when every digit is. The target is a weight: the digits are
    its view at the decimals the screen shows, and a change of the decimals over the API or the console keeps the
    weight (charge_mode_settings.c; review D2, 2026-11-16).
*/
static uint32_t charge_mode_digits_scale(decimal_places_t places) {
    return places == DP_3 ? 1000 : 100;
}


float charge_mode_target_at(decimal_places_t places) {
    uint32_t value = 0;
    for (int idx = 4; idx >= 0; idx -= 1) {
        value = value * 10 + charge_weight_digits[idx];
    }
    return (float) value / (float) charge_mode_digits_scale(places);
}


float charge_mode_get_target(void) {
    return charge_mode_target_at(charge_mode_config.eeprom_charge_mode_data.decimal_places);
}


bool charge_mode_target_fits_at(float target, decimal_places_t places) {
    double scaled = (double) target * charge_mode_digits_scale(places);
    double rounded = round(scaled);
    // What the digits can show: 0.01 to 999.99 at two decimals, 0.001 to 99.999 at three
    return isfinite(scaled) && fabs(scaled - rounded) <= 1e-3 && rounded >= 1 && rounded <= 99999;
}


bool charge_mode_target_fits(float target) {
    return charge_mode_target_fits_at(target, charge_mode_config.eeprom_charge_mode_data.decimal_places);
}


static void charge_mode_write_digits(float target, decimal_places_t places) {
    uint32_t value = (uint32_t) round((double) target * charge_mode_digits_scale(places));
    for (int idx = 0; idx < 5; idx += 1) {
        charge_weight_digits[idx] = (uint8_t) (value % 10);
        value /= 10;
    }
}


bool charge_mode_set_target(float target) {
    if (!charge_mode_target_fits(target)) {
        return false;
    }
    activity_lock();
    charge_mode_write_digits(target, charge_mode_config.eeprom_charge_mode_data.decimal_places);
    // A charge that runs throws it from its next throw on. Its own copy is taken under this lock as it begins
    // (charge_mode_menu), so a target written as a charge starts is never left out of it (review D2)
    if (activity_get() == ACTIVITY_CHARGE) {
        charge_mode_config.target_charge_weight = charge_mode_get_target();
    }
    activity_unlock();
    return true;
}


void charge_mode_keep_target(decimal_places_t before) {
    float target = charge_mode_target_at(before);
    if (target > 0.0f) {
        charge_mode_write_digits(target, charge_mode_config.eeprom_charge_mode_data.decimal_places);
    }
}


charge_start_result_t charge_mode_request_start(activity_by_t by, float target, int powder_profile) {
    // A job, a start asked already, or a person at the unit in a mode or a view of the menu's: not taken over
    activity_t asked_before;
    if (activity_get() != ACTIVITY_NONE || activity_start_pending(&asked_before) || menu_get_current_form_id() < 0) {
        return CHARGE_START_BUSY;
    }
    // The drivers looked at afresh, as a start at the unit does: a fault is remembered until a mode asks, so the
    // remembered one refused every start over the API after a driver's passing reset (found trying the review's fixes)
    if (!motors_ready()) {
        return CHARGE_START_DRIVER_FAULT;
    }
    if (isnan(target) ? charge_mode_get_target() <= 0.0f : !charge_mode_target_fits(target)) {
        return CHARGE_START_NO_TARGET;
    }
    // The staged controller takes every number from the powder's calibration: none, no start (ADR-009)
    if (charge_mode_config.eeprom_charge_mode_data.controller == CHARGE_CONTROLLER_STAGED) {
        uint8_t profile = powder_profile >= 0 ? (uint8_t) powder_profile : (uint8_t) profile_get_selected_idx();
        if (!charge_calibration_held(profile)) {
            return CHARGE_START_NOT_CALIBRATED;
        }
    }
    // The menu takes a start under the activity lock, so the target and the profile are in before it can
    activity_lock();
    bool asked = activity_request_start(ACTIVITY_CHARGE, by);
    if (asked) {
        if (!isnan(target)) {
            charge_mode_set_target(target);
        }
        if (powder_profile >= 0) {
            profile_select((uint8_t) powder_profile);
        }
    }
    activity_unlock();
    if (!asked) {
        return CHARGE_START_BUSY;
    }
    menu_wake();
    return CHARGE_START_OK;
}


void charge_mode_get_status(charge_status_t * status) {
    activity_t asked;
    bool running = activity_get() == ACTIVITY_CHARGE;
    if (running && charge_ended_on_fault) {
        status->state = "fault";
    }
    else if (!running && activity_start_pending(&asked) && asked == ACTIVITY_CHARGE) {
        status->state = "starting";
    }
    else if (!running) {
        status->state = "idle";
    }
    else {
        switch (charge_mode_config.charge_mode_state) {
            case CHARGE_MODE_WAIT_FOR_ZERO: status->state = "zeroing"; break;
            case CHARGE_MODE_WAIT_FOR_COMPLETE: status->state = "charging"; break;
            case CHARGE_MODE_WAIT_FOR_CUP_REMOVAL: status->state = "waiting_for_cup_removal"; break;
            case CHARGE_MODE_WAIT_FOR_CUP_RETURN: status->state = "waiting_for_cup_return"; break;
            default: status->state = "idle"; break;
        }
    }
    status->in_throw = activity_in_throw();
    status->throw_number = charge_throw_number;
    status->paused = charge_paused;
    status->target = charge_mode_get_target();
    status->elapsed_s = charge_mode_get_elapsed_s();
    status->last_throw = charge_last_throw;
    status->last_result = charge_last_result;
    status->last_reason = charge_last_reason;
    status->last_weight = charge_last_weight;
}


float charge_mode_get_elapsed_s(void) {
    if (charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_COMPLETE && charge_timer_running) {
        return (float) ((xTaskGetTickCount() - charge_start_tick) * portTICK_PERIOD_MS) / 1000.0f;
    }
    return last_charge_elapsed_seconds;
}
