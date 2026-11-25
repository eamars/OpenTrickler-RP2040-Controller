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
#include "settings.h"
#include "charge_controller.h"
#include "charge_tuner.h"
#include "pico/time.h"


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

    // The calibration's cup: never calibrated
    .cup_capacity = -1.0f,
    .cup_weight = -1.0f,

    .auto_tune = true,
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

/*
    ADR-009 step 2: the charge on the controller (charge_controller.c) and the auto-tuner (charge_tuner.c), kept for
    good: the weight a turn carried from throw to throw and the tuner's windows, in RAM, taken from the heap at the
    first charge (the Pico W has no static RAM to spare). The throw's room is borrowed from the heap while a charge
    runs. Both begin afresh at a start of the unit, at a change of powder or of the scale's
    unit, and the tuner when the profile's kp is changed elsewhere or the switch is turned
*/
typedef struct {
    charge_controller_t controller;
    charge_tuner_t tuner;
} charge_step2_t;
static charge_step2_t * step2 = NULL;           // from the heap at the first charge, kept from then on
static charge_room_t * charge_room = NULL;
static int charge_tuned_profile = -1;           // the powder they are for; -1 none yet
static char charge_tuned_unit[4] = "";          // the scale's unit then
static bool charge_tuning = false;              // the switch then
static float charge_profile_kp = NAN;           // the profile's kp as last seen or written
static double charge_kp = NAN;                  // the fine kp the next throw is thrown on: the tuner's, or the profile's
static double charge_kp_thrown = NAN;

// What the controller costs, since the unit started
static uint32_t charge_tick_most_us = 0;
static uint32_t charge_reading_most_us = 0;
static uint64_t charge_tick_sum_us = 0;
static uint64_t charge_reading_sum_us = 0;
static uint32_t charge_tick_count = 0;
static uint32_t charge_reading_count = 0;
static size_t charge_heap_low = SIZE_MAX;

// Step 1's scale never calibrated: the defaults the simulation takes where a part was never run (step2.py)
#define CHARGE_DEFAULT_SETTLE_S 1.0f
#define CHARGE_DEFAULT_LAG_FLOW 1.0f
#define CHARGE_DEFAULT_LAG_S 0.8f
// The cup never calibrated: a reading this far under zero is a lifted cup, as the cup's removal is told today
#define CHARGE_LIFT_UNCALIBRATED (-10.0f)


static void charge_step2_configure(profile_t * profile) {
    eeprom_charge_mode_data_t * data = &charge_mode_config.eeprom_charge_mode_data;
    const char * unit = scale_get_unit();
    int idx = (int) profile_get_selected_idx();
    bool same = idx == charge_tuned_profile && strncmp(unit ? unit : "", charge_tuned_unit, sizeof(charge_tuned_unit)) == 0;
    if (!same) {
        // Another powder or another unit of weight: nothing carried holds
        charge_controller_init(&step2->controller);
        charge_tuned_profile = idx;
        snprintf(charge_tuned_unit, sizeof(charge_tuned_unit), "%s", unit ? unit : "");
    }
    if (!same || data->auto_tune != charge_tuning || profile->fine_kp != charge_profile_kp) {
        charge_tuner_init(&step2->tuner, data->fine_stop_threshold);
        charge_tuning = data->auto_tune;
        charge_profile_kp = profile->fine_kp;
        charge_kp = profile->fine_kp;
        printf("Charge: the tuner begins afresh from kp %.4f\n", (double) profile->fine_kp);
    }
    step2->tuner.tolerance = data->fine_stop_threshold;
    if (!data->auto_tune) {
        charge_kp = profile->fine_kp;
    }

    charge_config_t * config = &step2->controller.config;
    config->coarse.kp = profile->coarse_kp;
    config->coarse.ki = profile->coarse_ki;
    config->coarse.kd = profile->coarse_kd;
    config->coarse.least_rps = fmaxf(get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR), profile->coarse_min_flow_speed_rps);
    config->coarse.most_rps = fminf(get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR), profile->coarse_max_flow_speed_rps);
    config->fine.kp = (float) charge_kp;
    config->fine.ki = profile->fine_ki;
    config->fine.kd = profile->fine_kd;
    config->fine.least_rps = fmaxf(get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR), profile->fine_min_flow_speed_rps);
    config->fine.most_rps = fminf(get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR), profile->fine_max_flow_speed_rps);
    config->coarse_threshold = data->coarse_stop_threshold;
    config->backoff_enable = data->coarse_stop_backoff_enable;
    config->backoff_turns = data->coarse_stop_backoff_turns;
    config->backoff_rps = data->coarse_stop_backoff_speed_rps;
    // Step 1's records, slowest flow first, those never measured left out; none, the defaults
    const eeprom_scale_data_t * scale = &scale_config.persistent_config;
    config->settle_s = scale->settle_ms > 0.0f ? scale->settle_ms / 1000.0f : CHARGE_DEFAULT_SETTLE_S;
    config->lag_count = 0;
    for (int i = 0; i < SCALE_LAG_POINTS && config->lag_count < CHARGE_LAG_POINTS; i++) {
        if (scale->lag[i].flow_per_s > 0.0f && scale->lag[i].lag_ms > 0.0f) {
            config->lag_flow_per_s[config->lag_count] = scale->lag[i].flow_per_s;
            config->lag_s[config->lag_count] = scale->lag[i].lag_ms / 1000.0f;
            config->lag_count++;
        }
    }
    if (config->lag_count == 0) {
        config->lag_flow_per_s[0] = CHARGE_DEFAULT_LAG_FLOW;
        config->lag_s[0] = CHARGE_DEFAULT_LAG_S;
        config->lag_count = 1;
    }
    config->acceleration_rps2 = motor_get_acceleration(SELECT_FINE_TRICKLER_MOTOR);
    charge_kp_thrown = charge_kp;
}


// The throw at rest, its verdict read: its own weight a turn carried, and its weight and time given to the tuner. A
// window closed, the next window's kp is the tuner's; a better kp kept is written into the powder's profile and stored
static void charge_step2_at_rest(float reading, float target, float elapsed_s) {
    charge_controller_at_rest(&step2->controller, reading);
    double carried;
    if (charge_controller_carried(&step2->controller, &carried)) {
        printf("Charge: carried weight a turn %.4f\n", carried);
    }
    if (!charge_mode_config.eeprom_charge_mode_data.auto_tune) {
        return;
    }
    double next_kp;
    charge_tuner_step_t step = charge_tuner_throw(&step2->tuner, charge_kp_thrown, (double) reading - target, elapsed_s,
                                                  &next_kp);
    if (step == CHARGE_TUNER_THROWING) {
        return;
    }
    static const char * const said[] = {"throwing", "first window", "resting", "the kept kp again", "kept",
                                        "put back, the other way", "put back both ways"};
    printf("Tuner: %s, kp %.4f; next %.4f, kept %.4f\n", said[step], charge_kp_thrown, next_kp, step2->tuner.base_kp);
    charge_kp = next_kp;
    if (step != CHARGE_TUNER_KEPT) {
        return;
    }
    char path[32];
    char text[16];
    snprintf(path, sizeof(path), "powder_profiles/%d", charge_tuned_profile);
    snprintf(text, sizeof(text), "%.4f", step2->tuner.base_kp);
    const char * why = settings_write_by(path, "fine_kp", text, ACTIVITY_BY_UNIT);
    why = why != NULL ? why : settings_store_by(path, ACTIVITY_BY_UNIT);
    if (why != NULL) {
        printf("Tuner: kp %s not kept, %s\n", text, why);
        return;
    }
    profile_t * profile = profile_get((uint16_t) charge_tuned_profile);
    charge_profile_kp = profile != NULL ? profile->fine_kp : NAN;
}


static void charge_cost_add(uint32_t us, uint32_t * most, uint64_t * sum, uint32_t * count) {
    *most = us > *most ? us : *most;
    *sum += us;
    *count += 1;
}


void charge_mode_get_cost(charge_cost_t * cost) {
    cost->tick_most_us = charge_tick_most_us;
    cost->tick_mean_us = charge_tick_count ? (uint32_t) (charge_tick_sum_us / charge_tick_count) : 0;
    cost->reading_most_us = charge_reading_most_us;
    cost->reading_mean_us = charge_reading_count ? (uint32_t) (charge_reading_sum_us / charge_reading_count) : 0;
    cost->heap_low = charge_heap_low == SIZE_MAX ? 0 : (uint32_t) charge_heap_low;
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

    // A zero asked, at the unit or over the API, starts the count of steady readings again: none from before it, or
    // from before it lands, makes the zero this throw pours from (ADR-003, gate 4)
    uint32_t zero_count = scale_get_zero_count();
    uint8_t readings_since_zero = 0;

    // Stop condition: 10 stable measurements 300 ms apart (3 seconds at least)
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

        // Perform measurement (waited for up to 300 ms)
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


void charge_mode_wait_for_complete() {

    charge_start_tick = xTaskGetTickCount();
    charge_timer_running = true;

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

    // ADR-009 step 2: the throw on the controller, given its config from the profile, the charge settings and step 1's
    // records, a tick each millisecond and each reading as it comes; it sets both tubes' speeds
    charge_controller_t * controller = &step2->controller;
    charge_step2_configure(profile_get_selected());
    if (!charge_controller_start(controller, charge_room, charge_mode_config.target_charge_weight,
                                 (double) time_us_64() / 1000.0)) {
        charge_mode_scale_fault("No memory", EVENT_REASON_FAULT, 0, NULL);
        return;
    }
    double carried;
    bool has_carried = charge_controller_carried(controller, &carried);
    printf("Charge: throw %u on fine kp %.4f, weight a turn %s%.4f\n", (unsigned) charge_throw_number,
           (double) controller->config.fine.kp, has_carried ? "carried " : "its own, none yet ",
           has_carried ? carried : 0.0);

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

    // A lifted cup is no light charge: a reading under half the empty cup's weight ends the throw (ADR-009 step 2)
    float cup_weight = charge_mode_config.eeprom_charge_mode_data.cup_weight;
    float lifted_under = cup_weight > 0.0f ? -cup_weight / 2.0f : CHARGE_LIFT_UNCALIBRATED;

    bool coarse_moved = true;
    double coarse_set = 0.0;
    double fine_set = 0.0;
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
        if (charge_mode_stop_asked()) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }
        // A motor driver that reports a fault, or no longer answers, ends the throw as a failed scale does (ADR-012)
        if (charge_mode_end_on_motor_fault()) {
            return;
        }

        uint32_t began_us = time_us_32();
        charge_controller_tick(controller, (double) time_us_64() / 1000.0);
        charge_cost_add(time_us_32() - began_us, &charge_tick_most_us, &charge_tick_sum_us, &charge_tick_count);

        float current_weight;
        if (scale_take_new_measurement(&current_weight)) {
            last_reading_tick = xTaskGetTickCount();
            if (current_weight < lifted_under) {
                printf("Charge: cup lifted, %.3f under %.3f\n", (double) current_weight, (double) lifted_under);
                charge_mode_scale_fault("Cup lifted", EVENT_REASON_CUP_LIFTED, 0, NULL);
                return;
            }
            began_us = time_us_32();
            charge_controller_reading(controller, (double) time_us_64() / 1000.0, current_weight);
            charge_cost_add(time_us_32() - began_us, &charge_reading_most_us, &charge_reading_sum_us,
                            &charge_reading_count);
            size_t heap_free = xPortGetFreeHeapSize();
            charge_heap_low = heap_free < charge_heap_low ? heap_free : charge_heap_low;
        }
        else if ((xTaskGetTickCount() - last_reading_tick) * portTICK_PERIOD_MS >= stale_ms) {
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
            // Both motors stood through the pause: counted from standing, and given the settled weight at once
            double now_ms = (double) time_us_64() / 1000.0;
            charge_controller_resume(controller, now_ms);
            charge_controller_reading(controller, now_ms, current_weight);
            coarse_set = 0.0;
            fine_set = 0.0;
            wake = xTaskGetTickCount();
        }

        // The speeds asked, set as they change
        if (controller->coarse_rps != coarse_set) {
            coarse_set = controller->coarse_rps;
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, (float) coarse_set);
        }
        if (controller->fine_rps != fine_set) {
            fine_set = controller->fine_rps;
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, (float) fine_set);
        }
        // The coarse tube stopped for good: the servo gate to its ratio (0.0 open, 1.0 closed), as today
        if (coarse_moved && !controller->coarse_moves) {
            coarse_moved = false;
            if (servo_gate_enabled()) {
                servo_gate_set_ratio(charge_mode_config.eeprom_charge_mode_data.coarse_stop_gate_ratio, false);
            }
        }
        if (controller->done) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            break;
        }
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

    // The verdict, read once the slowest lag and the settle time have passed after the stop (ADR-009 step 2): what
    // was poured has landed and shows
    vTaskDelay(pdMS_TO_TICKS((uint32_t) (charge_controller_verdict_after_s(&step2->controller) * 1000.0)));

    // Take current measurement
    float current_measurement = scale_get_current_measurement();
    float error = charge_mode_config.target_charge_weight - current_measurement;

    // Update LED colour before moving to the next stage
    // Over charged
    if (error <= -charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold) {
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
    else if (error >= charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold) {
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
    float fine_stop_threshold = charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;
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
    // Out of the throw, so a kp kept is written as any setting between throws
    charge_step2_at_rest(current_measurement, charge_mode_config.target_charge_weight, last_charge_elapsed_seconds);

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

    // Enable motor on entering the charge mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_ZERO;

    // The controller and the tuner, kept from the first charge on, and the room for each throw, borrowed while the
    // charge runs (ADR-009, gate 3); none to have, the charge ends at once and says why
    if (step2 == NULL) {
        step2 = (charge_step2_t *) pvPortMalloc(sizeof(charge_step2_t));
        if (step2 != NULL) {
            charge_controller_init(&step2->controller);
            charge_tuner_init(&step2->tuner, charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold);
            charge_tuned_profile = -1;
        }
    }
    charge_room = step2 != NULL ? (charge_room_t *) pvPortMalloc(sizeof(charge_room_t)) : NULL;
    if (charge_room == NULL) {
        printf("Charge: no room for the controller, %u and %u bytes\n", (unsigned) sizeof(charge_step2_t),
               (unsigned) sizeof(charge_room_t));
        charge_mode_scale_fault("No memory", EVENT_REASON_FAULT, 0, NULL);
    }

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
    if (charge_room != NULL) {
        vPortFree(charge_room);
        charge_room = NULL;
    }

    // The job is over, a throw cut short with it (ADR-003, gates 3 and 4): logged, unless a fault logged it
    charge_mode_throw_ended(EVENT_REASON_STOPPED, 0, NULL);
    charge_mode_log_stopped(charge_end_by, EVENT_REASON_STOPPED);
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
