#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <u8g2.h>

#include "app.h"
#include "FloatRingBuffer.h"
#include "mini_12864_module.h"
#include "display.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "neopixel_led.h"
#include "profile.h"
#include "servo_gate.h"
#include "common.h"
#include "powder_analysis.h"


// Memory from other modules
extern QueueHandle_t encoder_event_queue;
extern AppState_t exit_state;
extern charge_mode_config_t charge_mode_config;
extern neopixel_led_config_t neopixel_led_config;
extern scale_config_t scale_config;
extern servo_gate_t servo_gate;

// The result is written straight into a profile slot (name + 10 values) without selecting the slot.
// profile.h offers no access to an unselected slot, so the profile data is declared here, the same
// way mui_menu.c does it for the profile list.
extern eeprom_profile_data_t profile_data;


// Live state, shown by the display render task and the web interface.
// Three tasks touch this module, and with two cores they really run in parallel: the menu task
// runs powder_analysis_menu() and is the only writer of the status, the tuner and the recording;
// the render task reads the status for the display (and refreshes current_weight); the lwIP task
// answers the REST requests from a copy of the status (powder_analysis_get_status()). Only the
// two strings of the status are written inside a critical section: a torn number is one poll
// with an old or new value, a torn string could be unterminated. The trial protocol and the
// recording are append-only: an entry is complete before its counter is incremented, and the
// readers take the counter first.
static powder_analysis_status_t powder_analysis_status;
static powder_analysis_tuner_t tuner;

// Commands from the web interface
#define COMMAND_QUEUE_LENGTH 4
static QueueHandle_t command_queue = NULL;

// Display
static TaskHandle_t render_task_handler = NULL;

// Selection on the current screen: digit, list entry or footer button
static uint8_t cursor = 0;

// Target weight entry: hundredths .. hundreds, like charge_weight_digits of the charge mode
#define WEIGHT_DIGIT_COUNT 5
static uint8_t weight_digits[WEIGHT_DIGIT_COUNT];

// Where to continue when "Abort analysis?" is answered with No
static powder_analysis_state_t state_before_abort = POWDER_ANALYSIS_STATE_IDLE;

// Coarse trickler end-of-trickle backoff (copy of charge_mode.cpp)
#define COARSE_STOP_BACKOFF_MAX_MS 15000  // safety cap
static bool coarse_backoff_in_progress = false;
static TickType_t coarse_backoff_end_tick = 0;
static float coarse_backoff_speed_rps = 0.0f;     // for the trace only

// Per-sample recording of the last throw, see powder_analysis_trace_t
static powder_analysis_trace_t trace;

// Upper bound for gains of a test throw from the web (plausibility only)
#define TEST_THROW_MAX_GAIN 1000.0f

// Set by the REST handler when a web command made the menu task enter the module. Only then the
// queued START / TEST_THROW belongs to this entry; otherwise stale commands are dropped.
static bool entered_from_web = false;


// ---------------------------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------------------------
static float ticks_to_seconds(TickType_t ticks) {
    return (float) (ticks * portTICK_PERIOD_MS) / 1000.0f;
}


static void go_to_state(powder_analysis_state_t state, uint8_t initial_cursor) {
    powder_analysis_status.state = state;
    cursor = initial_cursor;
}


// The strings are read by the other tasks, see the note at the top
static void set_prompt(const char *text) {
    taskENTER_CRITICAL();
    snprintf(powder_analysis_status.prompt, sizeof(powder_analysis_status.prompt), "%s", text);
    taskEXIT_CRITICAL();
}


static void set_profile_name(const char *name) {
    taskENTER_CRITICAL();
    snprintf(powder_analysis_status.profile_name, sizeof(powder_analysis_status.profile_name), "%s", name);
    taskEXIT_CRITICAL();
}


static void stop_motors(void) {
    motor_set_speed(SELECT_BOTH_MOTOR, 0);
    coarse_backoff_in_progress = false;
}


// Before the first throw: both motors on, the servo gate (if used) opened once and left open
static void enable_tricklers(void) {
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);
    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_OPEN, true);
    }
}


static void set_led_colour(rgbw_u32_t colour) {
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        colour,
        colour,
        true
    );
}


static void reset_led_colour(void) {
    neopixel_led_set_colour(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour,
                            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour,
                            true);
}


// LED colour for the outcome of a throw, same colours as charge mode
static void show_evaluation_on_led(powder_analysis_evaluation_t evaluation) {
    switch (evaluation) {
        case POWDER_ANALYSIS_EVAL_OVER:
            set_led_colour(charge_mode_config.eeprom_charge_mode_data.neopixel_over_charge_colour);
            break;
        case POWDER_ANALYSIS_EVAL_HIT:
        case POWDER_ANALYSIS_EVAL_OK:
            set_led_colour(charge_mode_config.eeprom_charge_mode_data.neopixel_normal_charge_colour);
            break;
        default:
            set_led_colour(charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour);
            break;
    }
}


// Target weight <-> digits (2 decimal places, grains only)
static void weight_to_digits(float weight) {
    int hundredths = (int) roundf(weight * 100.0f);
    if (hundredths < 0) {
        hundredths = 0;
    }
    for (int i = 0; i < WEIGHT_DIGIT_COUNT; i++) {
        weight_digits[i] = hundredths % 10;
        hundredths /= 10;
    }
}


static float digits_to_weight(void) {
    return weight_digits[4] * 100 +
           weight_digits[3] * 10 +
           weight_digits[2] * 1 +
           weight_digits[1] * 0.1f +
           weight_digits[0] * 0.01f;
}


// Smallest "analysis_<n>" that is not used by any profile yet
static void make_profile_name(char *name, size_t name_len) {
    for (int n = 1; n < 1000; n++) {
        snprintf(name, name_len, "analysis_%d", n);

        bool used = false;
        for (int idx = 0; idx < MAX_PROFILE_CNT; idx++) {
            if (strncmp(profile_data.profiles[idx].name, name, PROFILE_NAME_MAX_LEN) == 0) {
                used = true;
            }
        }
        if (!used) {
            return;
        }
    }
}


// Short texts for the result screens, built from the config values so they never go stale.
// All fit the display (max 20 characters) with sensible config values.
// Percent a correction factor lowers a value by (0.9 -> 10)
static int correction_percent(float factor) {
    return (int) roundf((1.0f - factor) * 100.0f);
}


static void result_reason_text(const powder_analysis_result_t *result, char *buf, size_t len) {
    if (result->verified) {
        snprintf(buf, len, "Verified %d/%d%s", result->verify_hits, POWDER_ANALYSIS_VERIFY_ROUND_THROWS,
                 result->fine_corrections > 0 ? " corr." : "");
    }
    else if (result->trial_limit_reached) {
        snprintf(buf, len, "Trial limit reached");
    }
    else if (result->verify_overthrow_limit_reached) {
        snprintf(buf, len, "Overthrows, %d corr.", POWDER_ANALYSIS_MAX_FINE_CORRECTIONS);
    }
    else {
        snprintf(buf, len, "Not verified");
    }
}


static void coarse_hint_text(const powder_analysis_result_t *result, char *buf, size_t len) {
    if (result->coarse_kd_not_found) {
        snprintf(buf, len, "Coarse Kd too slow");
    }
    else if (result->coarse_overshoot_too_high) {
        snprintf(buf, len, "Overshoot > %.1f gr", POWDER_ANALYSIS_COARSE_MAX_OVERSHOOT_GR);
    }
    else if (result->coarse_time_goal_missed) {
        snprintf(buf, len, "Slower than %.0f s", POWDER_ANALYSIS_COARSE_TIME_GOAL_S);
    }
    else {
        snprintf(buf, len, "OK");
    }
}


// What a verification correction did, e.g. "Kp -10% (1/3)"
static void correction_text(const powder_analysis_status_t *status, char *buf, size_t len) {
    snprintf(buf, len, "%s -%d%% (%d/%d)",
             status->correction_lowered_kp ? "Kp" : "MinSpd",
             correction_percent(status->correction_lowered_kp ? POWDER_ANALYSIS_FINE_KP_CORRECTION_FACTOR
                                                              : POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR),
             status->fine_corrections, POWDER_ANALYSIS_MAX_FINE_CORRECTIONS);
}


// One line for the outcome of a throw, e.g. "23.12 8.3s HIT" (weight in gr)
static void throw_result_text(const powder_analysis_trial_t *trial, char *buf, size_t len) {
    snprintf(buf, len, "%.2f %.1fs %s", trial->weight, trial->total_time_s,
             powder_analysis_evaluation_to_string(trial->evaluation));
}


// ---------------------------------------------------------------------------------------------
// User input: display buttons and web commands
// ---------------------------------------------------------------------------------------------
static void ensure_command_queue(void) {
    if (command_queue == NULL) {
        command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(powder_analysis_command_t));
    }
}


// Read one input. Web commands wake a blocked read through OVERRIDE_FROM_REST in the encoder
// queue (see powder_analysis_post_command()), the command itself is taken from the command queue.
static void read_input(bool block, ButtonEncoderEvent_t *button, powder_analysis_command_t *command) {
    *button = button_wait_for_input(block);
    if (*button == OVERRIDE_FROM_REST) {
        *button = BUTTON_NO_EVENT;
    }

    memset(command, 0x0, sizeof(*command));
    if (command_queue != NULL) {
        xQueueReceive(command_queue, command, 0);
    }
}


// Rotary knob moves the cursor over `option_count` choices. Returns true when the encoder was pressed.
static bool move_cursor(ButtonEncoderEvent_t button, uint8_t option_count) {
    if (button == BUTTON_ENCODER_ROTATE_CW) {
        cursor = (cursor + 1) % option_count;
    }
    else if (button == BUTTON_ENCODER_ROTATE_CCW) {
        cursor = (cursor + option_count - 1) % option_count;
    }
    return button == BUTTON_ENCODER_PRESSED;
}


// Abort is possible at any time: the motors stop at once, then the user is asked to confirm
static void request_abort(void) {
    stop_motors();
    state_before_abort = powder_analysis_status.state;
    go_to_state(POWDER_ANALYSIS_STATE_ABORT_CONFIRM, 0);
    set_prompt("Abort analysis?");
}


// ---------------------------------------------------------------------------------------------
// Analysis control
// ---------------------------------------------------------------------------------------------
static void start_analysis(void) {
    powder_analysis_tuner_init(&tuner,
                               powder_analysis_status.shape,
                               powder_analysis_status.grain_size,
                               powder_analysis_status.target_weight,
                               charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold,
                               charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold);
    powder_analysis_status.phase = POWDER_ANALYSIS_PHASE_COARSE_KP;
    powder_analysis_status.has_last_trial = false;
    powder_analysis_status.has_result = false;

    enable_tricklers();
    powder_analysis_status.flooding_running = false;
    go_to_state(POWDER_ANALYSIS_STATE_FLOODING, 0);
    set_prompt("Fill the tubes");
}


static void set_flooding(bool running) {
    powder_analysis_status.flooding_running = running;
    if (running) {
        motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, POWDER_ANALYSIS_FLOOD_COARSE_SPEED_RPS);
        motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, POWDER_ANALYSIS_FLOOD_FINE_SPEED_RPS);
    }
    else {
        stop_motors();
    }
}


// After a throw (or the flooding) the pan has to be emptied before the next throw
static void start_emptying_pan(void) {
    go_to_state(POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL, 0);
    set_prompt("Empty pan into hopper");
}


static void finish_analysis(void) {
    stop_motors();
    reset_led_colour();

    powder_analysis_tuner_get_result(&tuner, &powder_analysis_status.result);
    powder_analysis_status.has_result = true;
    powder_analysis_status.phase = POWDER_ANALYSIS_PHASE_DONE;

    // Default save target: the first slot, under a fresh "analysis_<n>" name
    char name[PROFILE_NAME_MAX_LEN];
    make_profile_name(name, sizeof(name));
    powder_analysis_status.profile_idx = 0;
    set_profile_name(name);

    go_to_state(POWDER_ANALYSIS_STATE_RESULT_COARSE, 0);
    set_prompt("Analysis finished");
}


// Write the result into the chosen profile slot. Only the name and the 10 tuned values change,
// rev and compatibility of the slot are kept.
static void save_profile(void) {
    const powder_analysis_result_t *result = &powder_analysis_status.result;
    profile_t *slot = &profile_data.profiles[powder_analysis_status.profile_idx];

    strncpy(slot->name, powder_analysis_status.profile_name, PROFILE_NAME_MAX_LEN);
    slot->name[PROFILE_NAME_MAX_LEN - 1] = '\0';

    slot->coarse_kp = result->coarse_kp;
    slot->coarse_ki = result->coarse_ki;
    slot->coarse_kd = result->coarse_kd;
    slot->coarse_min_flow_speed_rps = result->coarse_min_flow_speed_rps;
    slot->coarse_max_flow_speed_rps = result->coarse_max_flow_speed_rps;

    slot->fine_kp = result->fine_kp;
    slot->fine_ki = result->fine_ki;
    slot->fine_kd = result->fine_kd;
    slot->fine_min_flow_speed_rps = result->fine_min_flow_speed_rps;
    slot->fine_max_flow_speed_rps = result->fine_max_flow_speed_rps;

    profile_data_save();
}


// SAVE from the web brings slot and name along and has asked for confirmation itself
static void save_from_web(const powder_analysis_command_t *command) {
    powder_analysis_status.profile_idx = command->profile_idx % MAX_PROFILE_CNT;
    set_profile_name(command->profile_name);
    save_profile();
    go_to_state(POWDER_ANALYSIS_STATE_ACTIVATE, 0);
}


// ---------------------------------------------------------------------------------------------
// Throw
// ---------------------------------------------------------------------------------------------
// Copy of coarse_trickler_backoff() from charge_mode.cpp, keep in sync
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
    coarse_backoff_speed_rps = -fabsf(backoff_speed);    // not in the charge mode copy: remembered for the trace
}


// ---------------------------------------------------------------------------------------------
// Per-sample recording of a throw (the last one is kept until the next throw starts)
// ---------------------------------------------------------------------------------------------
static void trace_start(const powder_analysis_trial_params_t *params) {
    trace.count = 0;
    trace.truncated = false;
    trace.final_weight = NAN;       // known once the weight has settled after the throw
    trace.target_weight = powder_analysis_status.target_weight;
    trace.test_throw = powder_analysis_status.test_mode;
    trace.params = *params;
}


static void trace_append(float t_s, float weight, float fine_p, float fine_d, float fine_speed,
                         float coarse_p, float coarse_d, float coarse_speed) {
    if (trace.count >= POWDER_ANALYSIS_TRACE_MAX_SAMPLES) {
        trace.truncated = true;     // the throw goes on, only the recording stops
        return;
    }

    powder_analysis_trace_sample_t *sample = &trace.samples[trace.count];
    sample->t_s = t_s;
    sample->weight = weight;
    sample->fine_p = fine_p;
    sample->fine_d = fine_d;
    sample->fine_speed = fine_speed;
    sample->coarse_p = coarse_p;
    sample->coarse_d = coarse_d;
    sample->coarse_speed = coarse_speed;
    trace.count += 1;       // after the sample: the REST handler reads up to count
}


// The sample that ends a throw is recorded as well, otherwise the trace would miss the stop
// itself (the stop conditions are checked before the speeds are updated). The motors are already
// stopped at that moment, so both commanded speeds are 0.
static void trace_append_stop_sample(float t_s, float weight, float fine_p, float fine_d, float coarse_p, float coarse_d) {
    trace_append(t_s, weight, fine_p, fine_d, 0.0f, coarse_p, coarse_d, 0.0f);
}


// The recording is complete once the settled weight is known. Only then the id changes, so the
// web page fetches the recording exactly once and with everything in it.
static void trace_finish(float final_weight) {
    trace.final_weight = final_weight;
    trace.id += 1;          // after the weight: the page fetches once the id changed
}


// Change of the error per millisecond for the D term, 0 on the first sample and on a zero interval
// (a plain division gave inf/NaN there and sent both tricklers to maximum speed). Same as in charge_mode.cpp.
static float error_derivative(float error, float last_error, float elapse_time_ms, bool first_sample) {
    if (first_sample || elapse_time_ms <= 0.0f) {
        return 0.0f;
    }
    return (error - last_error) / elapse_time_ms;
}


// Copy of the control loop of charge_mode_wait_for_complete() from charge_mode.cpp, keep in sync.
// Deliberate differences:
//   - gains, Ki (always 0) and flow speeds come from the trial params and the analysis config
//   - reset / abort command asks for confirmation instead of leaving the mode
//   - the servo gate was opened once at the start, no gate ratio at the coarse stop, no precharge,
//     no 20 ms delay at the end, no title / elapsed-time display handling
//   - a coarse-only throw ends after the coarse stop plus backoff; stop_motors() stops both
//     motors and clears the backoff flag
//   - safety timeout, coarse / total time measured, live elapsed time in the status
//   - errors and derivatives are computed before the stop checks so the stop sample can be
//     recorded (trace) and its weight reported (stop_weight)
// Returns false when the user aborted the throw.
static bool run_throw(const powder_analysis_trial_params_t *params, powder_analysis_trial_result_t *result) {
    memset(result, 0x0, sizeof(*result));
    trace_start(params);

    TickType_t trial_start_tick = xTaskGetTickCount();
    coarse_backoff_in_progress = false;

    // Set colour to under charge
    set_led_colour(charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour);

    // Find the minimum of max speed from the motor and the analysis config
    float coarse_trickler_max_speed = fmin(get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           POWDER_ANALYSIS_COARSE_MAX_FLOW_SPEED_RPS);
    float coarse_trickler_min_speed = fmax(get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           POWDER_ANALYSIS_COARSE_MIN_FLOW_SPEED_RPS);
    float fine_trickler_max_speed = fmin(get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         POWDER_ANALYSIS_FINE_MAX_FLOW_SPEED_RPS);
    // The fine minimum speed comes from the trial: the verification lowers it when kernels keep
    // falling after the stop. Lowering is safe: the motor's own minimum (0.01 rps by default in
    // motors.c) is still enforced here, and a trickler that no longer moves powder simply ends
    // the throw as TIMEOUT.
    float fine_trickler_min_speed = fmax(get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         params->fine_min_flow_speed_rps);

    // Define PID terms
    float coarse_trickler_integral = 0.0f;
    float fine_trickler_integral = 0.0f;
    float coarse_trickler_last_error = 0.0f;
    float fine_trickler_last_error = 0.0f;

    // Calculate target weight for coarse trickler
    // The coarse trickler is supposed to stop ahead of the target weight by an offset
    float target_weight = powder_analysis_status.target_weight;
    float coarse_trickler_target_charge_weight = fmaxf(0.0f, target_weight - charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold);

    TickType_t last_sample_tick = trial_start_tick;
    TickType_t current_sample_tick = last_sample_tick;
    TickType_t coarse_stop_tick = 0;    // 0 = the coarse trickler has not stopped yet
    bool should_coarse_trickler_move = true;
    bool first_sample = true;           // no D term before the second measurement
    bool coarse_only = (params->throw_type == POWDER_ANALYSIS_THROW_COARSE_ONLY);

    // Last sample, kept outside the loop for the trace of a timeout
    float current_weight = 0.0f;
    float fine_p = 0.0f;
    float fine_d = 0.0f;
    float coarse_p = 0.0f;
    float coarse_d = 0.0f;

    while (true) {
        // Non block waiting for the input
        ButtonEncoderEvent_t button;
        powder_analysis_command_t command;
        read_input(false, &button, &command);
        if (button == BUTTON_RST_PRESSED || command.type == POWDER_ANALYSIS_CMD_ABORT) {
            request_abort();
            return false;
        }

        // Safety timeout: a throw that takes this long has a far too small Kp
        powder_analysis_status.elapsed_s = ticks_to_seconds(xTaskGetTickCount() - trial_start_tick);
        if (powder_analysis_status.elapsed_s > POWDER_ANALYSIS_TRIAL_TIMEOUT_S) {
            stop_motors();
            result->timed_out = true;
            trace_append_stop_sample(powder_analysis_status.elapsed_s, current_weight, fine_p, fine_d, coarse_p, coarse_d);
            break;
        }

        // Run the PID controlled loop to start charging
        // Perform the measurement
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            // If no measurement within 200ms then poll the button and retry
            continue;
        }
        current_sample_tick = xTaskGetTickCount();
        float sample_time_s = ticks_to_seconds(current_sample_tick - trial_start_tick);

        // Handle coarse trickler backoff stop
        if (coarse_backoff_in_progress && (current_sample_tick >= coarse_backoff_end_tick)) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            coarse_backoff_in_progress = false;
        }

        // Errors and their change per millisecond (ahead of the stop checks, for the trace)
        float coarse_trickler_error = coarse_trickler_target_charge_weight - current_weight;
        float fine_trickler_error = target_weight - current_weight;
        float elapse_time_ms = (current_sample_tick - last_sample_tick) / portTICK_RATE_MS;
        float fine_trickler_derivative = error_derivative(fine_trickler_error, fine_trickler_last_error, elapse_time_ms, first_sample);
        float coarse_trickler_derivative = error_derivative(coarse_trickler_error, coarse_trickler_last_error, elapse_time_ms, first_sample);

        // A coarse-only throw is over once the coarse trickler has stopped and the backoff is done
        if (coarse_only && !should_coarse_trickler_move && !coarse_backoff_in_progress) {
            stop_motors();
            trace_append_stop_sample(sample_time_s, current_weight,
                                     params->fine_kp * fine_trickler_error, params->fine_kd * fine_trickler_derivative,
                                     0.0f, 0.0f);
            break;
        }

        // Fine & Coarse trickler stop condition
        if (fine_trickler_error < charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold) {
            // Stop all motors
            stop_motors();
            result->stop_weight = current_weight;   // what still falls after this is the "fall after stop"
            trace_append_stop_sample(sample_time_s, current_weight,
                                     params->fine_kp * fine_trickler_error, params->fine_kd * fine_trickler_derivative,
                                     should_coarse_trickler_move ? params->coarse_kp * coarse_trickler_error : 0.0f,
                                     should_coarse_trickler_move ? params->coarse_kd * coarse_trickler_derivative : 0.0f);
            break;
        }

        // Coarse trickler stop condition
        else if (coarse_trickler_error <= 0 &&
                 should_coarse_trickler_move) {

            should_coarse_trickler_move = false;
            coarse_stop_tick = current_sample_tick;
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            // Reverse the coarse trickler for a short backoff
            coarse_trickler_backoff(coarse_trickler_min_speed);
        }

        // Update fine trickler speed
        // (in a coarse-only throw all fine gains are 0, so the fine trickler runs at minimum speed like in real use)
        fine_trickler_integral += fine_trickler_error;

        float new_p = params->fine_kp * fine_trickler_error;
        float new_i = POWDER_ANALYSIS_KI * fine_trickler_integral;
        float new_d = params->fine_kd * fine_trickler_derivative;
        float new_speed = fmaxf(fine_trickler_min_speed, fminf(new_p + new_i + new_d, fine_trickler_max_speed));
        motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, new_speed);

        // For the trace (not in the charge mode copy)
        fine_p = new_p;
        fine_d = new_d;
        float fine_speed = new_speed;
        coarse_p = 0.0f;
        coarse_d = 0.0f;
        float coarse_speed = coarse_backoff_in_progress ? coarse_backoff_speed_rps : 0.0f;

        // Update coarse trickler speed
        if (should_coarse_trickler_move && !coarse_backoff_in_progress) {
            coarse_trickler_integral += coarse_trickler_error;

            new_p = params->coarse_kp * coarse_trickler_error;
            new_i = POWDER_ANALYSIS_KI * coarse_trickler_integral;
            new_d = params->coarse_kd * coarse_trickler_derivative;

            new_speed = fmaxf(coarse_trickler_min_speed, fminf(new_p + new_i + new_d, coarse_trickler_max_speed));

            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, new_speed);

            coarse_p = new_p;
            coarse_d = new_d;
            coarse_speed = new_speed;
        }

        trace_append(sample_time_s, current_weight, fine_p, fine_d, fine_speed, coarse_p, coarse_d, coarse_speed);

        // Record state
        last_sample_tick = current_sample_tick;
        fine_trickler_last_error = fine_trickler_error;
        coarse_trickler_last_error = coarse_trickler_error;
        first_sample = false;
    }

    // Times: coarse = start to coarse stop, total = start to the end of the throw
    TickType_t end_tick = xTaskGetTickCount();
    if (coarse_stop_tick == 0) {
        coarse_stop_tick = end_tick;    // the coarse trickler ran until the end (timeout or overthrow)
    }
    result->coarse_time_s = ticks_to_seconds(coarse_stop_tick - trial_start_tick);
    result->total_time_s = ticks_to_seconds(end_tick - trial_start_tick);
    if (coarse_only) {
        result->total_time_s = result->coarse_time_s;   // the backoff does not count
    }
    powder_analysis_status.elapsed_s = result->total_time_s;

    return true;        // the recording is completed by the caller once the weight has settled
}


// Wait until the scale reading has settled and return it (same criteria as charge mode).
// Returns false when the user aborted.
static bool wait_for_stable_weight(float *weight) {
    FloatRingBuffer data_buffer(5);
    TickType_t wait_start_tick = xTaskGetTickCount();

    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        ButtonEncoderEvent_t button;
        powder_analysis_command_t command;
        read_input(false, &button, &command);
        if (button == BUTTON_RST_PRESSED || command.type == POWDER_ANALYSIS_CMD_ABORT) {
            request_abort();
            return false;
        }

        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            continue;
        }
        data_buffer.enqueue(current_weight);

        if (data_buffer.getCounter() >= 5) {
            bool stable = data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin;
            bool waited_long_enough = ticks_to_seconds(xTaskGetTickCount() - wait_start_tick) > POWDER_ANALYSIS_STABLE_WEIGHT_TIMEOUT_S;
            if (stable || waited_long_enough) {
                *weight = data_buffer.getMean();
                return true;
            }
        }

        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(300));
    }
}


// One trial: throw with the gains the tuner wants to try, measure, let the tuner judge
static void run_trial(void) {
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_get_trial(&tuner, &params);
    powder_analysis_status.trial = params;
    powder_analysis_status.phase = params.phase;
    set_prompt("Throw running");

    powder_analysis_trial_result_t result;
    if (!run_throw(&params, &result)) {
        return;     // aborted, the abort question is shown now
    }

    if (!wait_for_stable_weight(&result.weight)) {
        return;
    }
    trace_finish(result.weight);

    uint8_t corrections_before = tuner.fine_corrections;
    powder_analysis_evaluation_t evaluation = powder_analysis_tuner_report_result(&tuner, &result);
    show_evaluation_on_led(evaluation);

    // Remember the throw for the result screen
    powder_analysis_status.last_trial = tuner.trials[tuner.trial_count - 1];
    powder_analysis_status.has_last_trial = true;
    powder_analysis_status.phase = tuner.phase;

    // Verification progress (round number stays 0 until the verification starts)
    powder_analysis_status.verify_round = tuner.verify_reached ? tuner.verify_round.number : 0;
    powder_analysis_status.verify_round_hits = tuner.verify_round.hit_count;
    powder_analysis_status.fine_corrections = tuner.fine_corrections;
    powder_analysis_status.correction_just_applied = (tuner.fine_corrections > corrections_before);
    powder_analysis_status.correction_lowered_kp = tuner.last_correction_lowered_kp;

    if (powder_analysis_tuner_is_done(&tuner)) {
        finish_analysis();
    }
    else {
        start_emptying_pan();
    }
}


// ---------------------------------------------------------------------------------------------
// Test throw: one full throw with values from the web, recorded per sample. The tuner and the
// analysis result are not touched.
// ---------------------------------------------------------------------------------------------
static void start_test_throw(const powder_analysis_command_t *command, bool pan_has_powder) {
    powder_analysis_status.test_mode = true;
    powder_analysis_status.target_weight = command->target_weight;
    powder_analysis_status.has_last_trial = false;
    powder_analysis_status.phase = POWDER_ANALYSIS_PHASE_DONE;      // not part of a search

    // The values of the throw travel in the same struct the analysis uses
    powder_analysis_trial_params_t *params = &powder_analysis_status.trial;
    memset(params, 0x0, sizeof(*params));
    params->phase = POWDER_ANALYSIS_PHASE_DONE;      // not part of a search
    params->throw_type = POWDER_ANALYSIS_THROW_FULL;
    params->coarse_kp = command->coarse_kp;
    params->coarse_kd = command->coarse_kd;
    params->fine_kp = command->fine_kp;
    params->fine_kd = command->fine_kd;
    params->fine_min_flow_speed_rps = command->fine_min_flow_speed_rps;

    enable_tricklers();
    if (pan_has_powder) {
        start_emptying_pan();       // after a previous test throw
    }
    else {
        go_to_state(POWDER_ANALYSIS_STATE_WAIT_ZERO, 0);
        set_prompt("Put empty pan on scale");
    }
}


// Hit / over / under against the fine stop threshold, no time rule for a test throw
static powder_analysis_evaluation_t evaluate_test_throw(float weight) {
    float threshold = charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;
    if (weight > powder_analysis_status.target_weight + threshold) {
        return POWDER_ANALYSIS_EVAL_OVER;
    }
    if (weight < powder_analysis_status.target_weight - threshold) {
        return POWDER_ANALYSIS_EVAL_UNDER;
    }
    return POWDER_ANALYSIS_EVAL_HIT;
}


static void run_test_throw(void) {
    set_prompt("Test throw running");

    powder_analysis_trial_result_t result;
    if (!run_throw(&powder_analysis_status.trial, &result)) {
        return;     // aborted
    }
    if (!wait_for_stable_weight(&result.weight)) {
        return;
    }
    trace_finish(result.weight);

    powder_analysis_evaluation_t evaluation = evaluate_test_throw(result.weight);
    show_evaluation_on_led(evaluation);

    // Shown like an analysis throw (number 0 marks a test throw)
    powder_analysis_trial_t *last = &powder_analysis_status.last_trial;
    memset(last, 0x0, sizeof(*last));
    last->phase = POWDER_ANALYSIS_PHASE_DONE;
    last->kp = powder_analysis_status.trial.fine_kp;
    last->kd = powder_analysis_status.trial.fine_kd;
    last->weight = result.weight;
    last->coarse_time_s = result.coarse_time_s;
    last->total_time_s = result.total_time_s;
    last->evaluation = evaluation;
    last->fine_min_flow_speed_rps = powder_analysis_status.trial.fine_min_flow_speed_rps;
    powder_analysis_status.has_last_trial = true;

    go_to_state(POWDER_ANALYSIS_STATE_TEST_DONE, 0);
    set_prompt("Test throw done");
}


// ---------------------------------------------------------------------------------------------
// Emptying the pan between throws (copies of the charge mode waiting steps)
// ---------------------------------------------------------------------------------------------
// Input while waiting: encoder = pause, reset = abort. Returns true when the state was changed.
static bool handle_waiting_input(void) {
    ButtonEncoderEvent_t button;
    powder_analysis_command_t command;
    read_input(false, &button, &command);

    if (button == BUTTON_ENCODER_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_PAUSE_RESUME;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_ABORT;
    }

    if (command.type == POWDER_ANALYSIS_CMD_PAUSE_RESUME) {
        go_to_state(POWDER_ANALYSIS_STATE_PAUSED, 0);
        set_prompt("Paused, refill hopper");
        return true;
    }
    if (command.type == POWDER_ANALYSIS_CMD_ABORT) {
        request_abort();
        return true;
    }
    return false;
}


// Copy of charge_mode_wait_for_cup_removal(): the pan is removed when the scale shows its
// negative weight (well below zero) and the reading is stable
static void wait_for_pan_removal(void) {
    FloatRingBuffer data_buffer(5);

    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        if (handle_waiting_input()) {
            return;
        }

        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            continue;
        }
        data_buffer.enqueue(current_weight);

        if (data_buffer.getCounter() >= 5) {
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin &&
                data_buffer.getMean() + 10 < charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin) {
                break;
            }
        }

        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(300));
    }

    go_to_state(POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN, 0);
    set_prompt("Put the pan back");
}


// Copy of charge_mode_wait_for_cup_return(): the pan is back once the reading is not negative anymore
static void wait_for_pan_return(void) {
    // Like charge mode: the LED shows the outcome of the throw while the pan is away, "not ready" afterwards
    set_led_colour(charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour);

    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        if (handle_waiting_input()) {
            return;
        }

        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            continue;
        }

        if (current_weight >= 0) {
            break;
        }

        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(20));
    }

    go_to_state(POWDER_ANALYSIS_STATE_WAIT_ZERO, 0);
    set_prompt("Waiting for zero");
}


// Copy of charge_mode_wait_for_zero(): 10 stable readings around zero, then the next throw starts
static void wait_for_zero(void) {
    FloatRingBuffer data_buffer(10);

    while (true) {
        TickType_t last_measurement_tick = xTaskGetTickCount();

        if (handle_waiting_input()) {
            return;
        }

        float current_measurement;
        if (scale_block_wait_for_next_measurement(300, &current_measurement)) {
            data_buffer.enqueue(current_measurement);
        }

        if (data_buffer.getCounter() >= 10) {
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin &&
                fabsf(data_buffer.getMean()) < charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin) {
                break;
            }
        }

        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(300));
    }

    go_to_state(powder_analysis_status.test_mode ? POWDER_ANALYSIS_STATE_TEST_THROW : POWDER_ANALYSIS_STATE_TRIAL, 0);
}


// ---------------------------------------------------------------------------------------------
// Screens driven by buttons only (one input per call)
// ---------------------------------------------------------------------------------------------
static void screen_setup_weight(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Rotary changes the digit under the cursor, the encoder moves to the next digit and finally to "Next"
    if (cursor < WEIGHT_DIGIT_COUNT) {
        uint8_t *digit = &weight_digits[WEIGHT_DIGIT_COUNT - 1 - cursor];     // left to right
        if (button == BUTTON_ENCODER_ROTATE_CW) {
            *digit = (*digit + 1) % 10;
        }
        else if (button == BUTTON_ENCODER_ROTATE_CCW) {
            *digit = (*digit + 9) % 10;
        }
        else if (button == BUTTON_ENCODER_PRESSED) {
            cursor += 1;
        }
    }
    else {
        if (button == BUTTON_ENCODER_ROTATE_CCW) {
            cursor -= 1;    // back to the last digit
        }
        else if (button == BUTTON_ENCODER_PRESSED) {
            command.type = POWDER_ANALYSIS_CMD_NEXT;
        }
    }
    if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        float weight = digits_to_weight();
        if (weight > 0.0f) {
            powder_analysis_status.target_weight = weight;
            go_to_state(POWDER_ANALYSIS_STATE_SETUP_SHAPE, powder_analysis_status.shape);
        }
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


static void screen_setup_shape(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (move_cursor(button, POWDER_SHAPE_COUNT)) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        powder_analysis_status.shape = (powder_shape_t) cursor;
        go_to_state(POWDER_ANALYSIS_STATE_SETUP_GRAIN, powder_analysis_status.grain_size);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_SETUP_WEIGHT, 0);
    }
}


static void screen_setup_grain(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (move_cursor(button, GRAIN_SIZE_COUNT)) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        powder_analysis_status.grain_size = (grain_size_t) cursor;
        go_to_state(POWDER_ANALYSIS_STATE_PREPARE, 0);
        set_prompt("Put pan on scale");
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_SETUP_SHAPE, powder_analysis_status.shape);
    }
}


static void screen_prepare(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (button == BUTTON_ENCODER_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        start_analysis();
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_SETUP_GRAIN, powder_analysis_status.grain_size);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_ABORT) {
        request_abort();            // the web offers Abort here as well
    }
}


static void screen_flooding(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [Start/Pause] [Done], reset = abort
    if (move_cursor(button, 2)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_PAUSE_RESUME : POWDER_ANALYSIS_CMD_FLOOD_DONE;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_ABORT;
    }

    if (command.type == POWDER_ANALYSIS_CMD_PAUSE_RESUME) {
        set_flooding(!powder_analysis_status.flooding_running);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_FLOOD_DONE) {
        set_flooding(false);
        start_emptying_pan();
    }
    else if (command.type == POWDER_ANALYSIS_CMD_ABORT) {
        request_abort();
    }
}


static void screen_paused(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [Resume] [Zero] (Zero only if the scale driver can do it), reset = abort
    uint8_t option_count = powder_analysis_status.zero_available ? 2 : 1;
    if (move_cursor(button, option_count)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_PAUSE_RESUME : POWDER_ANALYSIS_CMD_ZERO_SCALE;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_ABORT;
    }

    if (command.type == POWDER_ANALYSIS_CMD_PAUSE_RESUME) {
        // The pan may or may not have been emptied meanwhile: waiting for zero covers both
        go_to_state(POWDER_ANALYSIS_STATE_WAIT_ZERO, 0);
        set_prompt("Empty pan into hopper");
    }
    else if (command.type == POWDER_ANALYSIS_CMD_ZERO_SCALE) {
        // Emergency zero on request only. Never done automatically: powder left in the pan would be zeroed away.
        if (powder_analysis_status.zero_available) {
            scale_config.scale_handle->force_zero();
        }
    }
    else if (command.type == POWDER_ANALYSIS_CMD_ABORT) {
        request_abort();
    }
}


static void screen_abort_confirm(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [No] [Yes], reset = no
    if (move_cursor(button, 2)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_ABORT_CANCEL : POWDER_ANALYSIS_CMD_ABORT_CONFIRM;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_ABORT_CANCEL;
    }

    if (command.type == POWDER_ANALYSIS_CMD_ABORT_CONFIRM) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_ABORT_CANCEL) {
        // The motors were stopped when abort was requested
        if (state_before_abort == POWDER_ANALYSIS_STATE_TRIAL || state_before_abort == POWDER_ANALYSIS_STATE_TEST_THROW) {
            start_emptying_pan();   // the interrupted throw is worthless, it is repeated (test mode stays)
        }
        else if (state_before_abort == POWDER_ANALYSIS_STATE_FLOODING) {
            powder_analysis_status.flooding_running = false;
            go_to_state(POWDER_ANALYSIS_STATE_FLOODING, 0);
        }
        else {
            go_to_state(state_before_abort, 0);
        }
    }
}


static void screen_result_coarse(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (button == BUTTON_ENCODER_PRESSED || button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        go_to_state(POWDER_ANALYSIS_STATE_RESULT_FINE, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_SAVE) {
        save_from_web(&command);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_DISCARD) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


static void screen_result_fine(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (button == BUTTON_ENCODER_PRESSED || button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        go_to_state(POWDER_ANALYSIS_STATE_RESULT_THRESHOLD, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_SAVE) {
        save_from_web(&command);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_DISCARD) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


static void screen_result_threshold(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [Save] [Discard], reset = discard
    if (move_cursor(button, 2)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_NEXT : POWDER_ANALYSIS_CMD_DISCARD;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_DISCARD;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        go_to_state(POWDER_ANALYSIS_STATE_SAVE_SLOT, powder_analysis_status.profile_idx);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_SAVE) {
        save_from_web(&command);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_DISCARD) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


static void screen_save_slot(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    if (move_cursor(button, MAX_PROFILE_CNT)) {
        command.type = POWDER_ANALYSIS_CMD_NEXT;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        powder_analysis_status.profile_idx = cursor;
        go_to_state(POWDER_ANALYSIS_STATE_SAVE_CONFIRM, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_RESULT_THRESHOLD, 0);
    }
}


static void screen_save_confirm(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [Back] [Save], reset = back
    if (move_cursor(button, 2)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_BACK : POWDER_ANALYSIS_CMD_NEXT;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_NEXT) {
        save_profile();
        go_to_state(POWDER_ANALYSIS_STATE_ACTIVATE, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK) {
        go_to_state(POWDER_ANALYSIS_STATE_SAVE_SLOT, powder_analysis_status.profile_idx);
    }
}


static void screen_activate(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Buttons: [No] [Yes], reset = no
    if (move_cursor(button, 2)) {
        command.type = (cursor == 0) ? POWDER_ANALYSIS_CMD_USE_NOW_NO : POWDER_ANALYSIS_CMD_USE_NOW_YES;
    }
    else if (button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_USE_NOW_NO;
    }

    if (command.type == POWDER_ANALYSIS_CMD_USE_NOW_YES) {
        profile_select(powder_analysis_status.profile_idx);
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
    else if (command.type == POWDER_ANALYSIS_CMD_USE_NOW_NO) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


static void screen_test_done(ButtonEncoderEvent_t button, powder_analysis_command_t command) {
    // Display: encoder or reset = close. Web: Close (BACK), or the next test throw right away.
    if (button == BUTTON_ENCODER_PRESSED || button == BUTTON_RST_PRESSED) {
        command.type = POWDER_ANALYSIS_CMD_BACK;
    }

    if (command.type == POWDER_ANALYSIS_CMD_TEST_THROW) {
        start_test_throw(&command, true);       // the pan still holds the last throw
    }
    else if (command.type == POWDER_ANALYSIS_CMD_BACK || command.type == POWDER_ANALYSIS_CMD_NEXT ||
             command.type == POWDER_ANALYSIS_CMD_ABORT || command.type == POWDER_ANALYSIS_CMD_ABORT_CONFIRM) {
        go_to_state(POWDER_ANALYSIS_STATE_EXIT, 0);
    }
}


// Block for one input and hand it to the current screen
static void handle_screen_input(void) {
    ButtonEncoderEvent_t button;
    powder_analysis_command_t command;
    read_input(true, &button, &command);

    switch (powder_analysis_status.state) {
        case POWDER_ANALYSIS_STATE_SETUP_WEIGHT:
            screen_setup_weight(button, command);
            break;
        case POWDER_ANALYSIS_STATE_SETUP_SHAPE:
            screen_setup_shape(button, command);
            break;
        case POWDER_ANALYSIS_STATE_SETUP_GRAIN:
            screen_setup_grain(button, command);
            break;
        case POWDER_ANALYSIS_STATE_PREPARE:
            screen_prepare(button, command);
            break;
        case POWDER_ANALYSIS_STATE_FLOODING:
            screen_flooding(button, command);
            break;
        case POWDER_ANALYSIS_STATE_PAUSED:
            screen_paused(button, command);
            break;
        case POWDER_ANALYSIS_STATE_ABORT_CONFIRM:
            screen_abort_confirm(button, command);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_COARSE:
            screen_result_coarse(button, command);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_FINE:
            screen_result_fine(button, command);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_THRESHOLD:
            screen_result_threshold(button, command);
            break;
        case POWDER_ANALYSIS_STATE_SAVE_SLOT:
            screen_save_slot(button, command);
            break;
        case POWDER_ANALYSIS_STATE_SAVE_CONFIRM:
            screen_save_confirm(button, command);
            break;
        case POWDER_ANALYSIS_STATE_ACTIVATE:
            screen_activate(button, command);
            break;
        case POWDER_ANALYSIS_STATE_TEST_DONE:
            screen_test_done(button, command);
            break;
        default:
            break;
    }
}


// ---------------------------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------------------------
// Layout: title and line like charge mode, three text rows (profont11 at TEXT_X fits 20
// characters) and optionally two footer buttons like the MUI menus. One-off positions (digits of
// the target weight) stay in their drawing function.
#define TEXT_X          5
#define TITLE_Y         10
#define TITLE_LINE_Y    13
#define FIRST_ROW_Y     25
#define ROW_HEIGHT      10
#define FOOTER_Y        59
#define FOOTER_MARGIN   3       // between a footer button and the display edge

static void draw_title(u8g2_t *display, const char *title) {
    u8g2_SetFont(display, u8g2_font_helvB08_tr);
    u8g2_DrawStr(display, TEXT_X, TITLE_Y, title);
    u8g2_DrawHLine(display, 0, TITLE_LINE_Y, u8g2_GetDisplayWidth(display));
}


static void draw_row(u8g2_t *display, uint8_t row, const char *text) {
    u8g2_SetFont(display, u8g2_font_profont11_tf);
    u8g2_DrawStr(display, TEXT_X, FIRST_ROW_Y + row * ROW_HEIGHT, text);
}


// Two footer buttons like the MUI menus. `selected` (0 or 1) is drawn inverted, -1 = no selection.
// An empty label leaves that button out.
static void draw_footer(u8g2_t *display, const char *left, const char *right, int selected) {
    u8g2_SetFont(display, u8g2_font_helvR08_tr);
    u8g2_uint_t flags = U8G2_BTN_HCENTER | U8G2_BTN_BW1;
    // The buttons are anchored to the display edges, so long labels are never clipped
    if (strlen(left) > 0) {
        u8g2_uint_t centre = FOOTER_MARGIN + u8g2_GetStrWidth(display, left) / 2;
        u8g2_DrawButtonUTF8(display, centre, FOOTER_Y, flags | (selected == 0 ? U8G2_BTN_INV : 0), 0, 1, 1, left);
    }
    if (strlen(right) > 0) {
        u8g2_uint_t centre = u8g2_GetDisplayWidth(display) - FOOTER_MARGIN - u8g2_GetStrWidth(display, right) / 2;
        u8g2_DrawButtonUTF8(display, centre, FOOTER_Y, flags | (selected == 1 ? U8G2_BTN_INV : 0), 0, 1, 1, right);
    }
}


// List of names, the cursor line is marked. `first` is the first visible entry.
static void draw_list(u8g2_t *display, const char * const *names, uint8_t count, uint8_t first, uint8_t rows) {
    char buf[32];
    for (uint8_t row = 0; row < rows && first + row < count; row++) {
        uint8_t idx = first + row;
        snprintf(buf, sizeof(buf), "%c %s", idx == cursor ? '>' : ' ', names[idx]);
        draw_row(display, row, buf);
    }
}


static void draw_weight_row(u8g2_t *display, uint8_t row, const char *label) {
    char buf[32];
    char weight_string[WEIGHT_STRING_LEN];
    float_to_string(weight_string, powder_analysis_status.current_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);
    snprintf(buf, sizeof(buf), "%s %s gr", label, weight_string);
    draw_row(display, row, buf);
}


// D1: target weight digits in the layout of the charge weight menu, cursor underlined. The digit
// positions are those of the charge weight menu (profont22, decimal point at 64), one-off here.
static void draw_setup_weight(u8g2_t *display) {
    const uint8_t digit_x[WEIGHT_DIGIT_COUNT] = {20, 36, 52, 76, 92};   // hundreds .. hundredths
    char buf[4];

    draw_title(display, "Target Weight");

    u8g2_SetFont(display, u8g2_font_profont22_tf);
    for (uint8_t i = 0; i < WEIGHT_DIGIT_COUNT; i++) {
        snprintf(buf, sizeof(buf), "%d", weight_digits[WEIGHT_DIGIT_COUNT - 1 - i]);
        u8g2_DrawStr(display, digit_x[i], 35, buf);
        if (cursor == i) {
            u8g2_DrawHLine(display, digit_x[i], 38, 12);
        }
    }
    u8g2_DrawStr(display, 64, 35, ".");

    u8g2_SetFont(display, u8g2_font_helvR08_tr);
    u8g2_DrawStr(display, 106, 35, "gr");

    draw_footer(display, "Back", "Next", cursor == WEIGHT_DIGIT_COUNT ? 1 : -1);
}


// D4: stop thresholds of the charge mode the analysis tunes for (C = coarse, F = fine). A fine
// threshold on an exact scale step is marked and the row for the pan hint makes room for a tip.
static void draw_prepare(u8g2_t *display) {
    char buf[32];
    float coarse_stop = charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;
    float fine_stop = charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;
    float resolution = POWDER_ANALYSIS_SCALE_RESOLUTION_GR;

    draw_title(display, "Prepare");
    if (powder_analysis_is_exact_scale_step(fine_stop)) {
        snprintf(buf, sizeof(buf), "Stop C %.1f F %.2f!", coarse_stop, fine_stop);
        draw_row(display, 0, buf);
        snprintf(buf, sizeof(buf), "F: use %.2f, %.2f..", 1.5f * resolution, 2.5f * resolution);
        draw_row(display, 1, buf);
        draw_row(display, 2, "Put pan on, zero it");
    }
    else {
        snprintf(buf, sizeof(buf), "Stop: C %.1f F %.2f", coarse_stop, fine_stop);
        draw_row(display, 0, buf);
        draw_row(display, 1, "Put pan on scale,");
        draw_row(display, 2, "zero scale if needed");
    }
    draw_footer(display, "Back", "Next", -1);
}


static void draw_flooding(u8g2_t *display) {
    draw_title(display, "Flooding");
    draw_weight_row(display, 0, "Weight");
    draw_row(display, 1, powder_analysis_status.flooding_running ? "Running" : "Stopped");
    draw_row(display, 2, "Reset = Abort");
    draw_footer(display, powder_analysis_status.flooding_running ? "Pause" : "Start", "Done", cursor);
}


// D6 / D8: empty pan prompt, with the result of the last throw on top if there is one
static void draw_empty_pan(u8g2_t *display) {
    char buf[32];
    if (powder_analysis_status.has_last_trial) {
        const powder_analysis_trial_t *trial = &powder_analysis_status.last_trial;
        if (powder_analysis_status.test_mode) {
            draw_title(display, "Test throw");
        }
        else {
            snprintf(buf, sizeof(buf), "#%d %s", trial->number, powder_analysis_phase_to_string(trial->phase));
            draw_title(display, buf);
        }
        throw_result_text(trial, buf, sizeof(buf));
        draw_row(display, 0, buf);
    }
    else {
        draw_title(display, "Empty Pan");
        draw_weight_row(display, 0, "Weight");
    }

    // A correction after a given up verification round is worth a line of its own
    if (powder_analysis_status.correction_just_applied) {
        correction_text(&powder_analysis_status, buf, sizeof(buf));
        draw_row(display, 1, buf);
        draw_row(display, 2, "Empty pan, put back");
    }
    else {
        draw_row(display, 1, "Empty pan to hopper");
        draw_row(display, 2, "and put it back");
    }
    draw_footer(display, "Pause", "Abort", -1);
}


static void draw_trial(u8g2_t *display) {
    char buf[32];
    const powder_analysis_trial_params_t *trial = &powder_analysis_status.trial;

    if (powder_analysis_status.test_mode) {
        draw_title(display, "Test throw");
    }
    else {
        snprintf(buf, sizeof(buf), "#%d %s", trial->trial_number, powder_analysis_phase_to_string(trial->phase));
        draw_title(display, buf);
    }

    // The gains of the trickler that is being tuned
    bool coarse_phase = (trial->phase == POWDER_ANALYSIS_PHASE_COARSE_KP || trial->phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    snprintf(buf, sizeof(buf), "Kp %.4g Kd %.3g",
             coarse_phase ? trial->coarse_kp : trial->fine_kp,
             coarse_phase ? trial->coarse_kd : trial->fine_kd);
    draw_row(display, 0, buf);

    // Weight against the target of the running throw
    char weight_string[WEIGHT_STRING_LEN];
    float_to_string(weight_string, powder_analysis_status.current_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);
    snprintf(buf, sizeof(buf), "%s / %.2f gr", weight_string, powder_analysis_status.target_weight);
    draw_row(display, 1, buf);

    snprintf(buf, sizeof(buf), "Time %.1f s", powder_analysis_status.elapsed_s);
    draw_row(display, 2, buf);
    draw_footer(display, "", "Abort", -1);
}


static void draw_paused(u8g2_t *display) {
    draw_title(display, "Paused");
    draw_weight_row(display, 0, "Weight");
    draw_row(display, 1, "Refill hopper if");
    draw_row(display, 2, "needed. RST = Abort");
    draw_footer(display, "Resume", powder_analysis_status.zero_available ? "Zero" : "", cursor);
}


static void draw_abort_confirm(u8g2_t *display) {
    draw_title(display, "Abort?");
    draw_row(display, 0, "Abort analysis?");
    draw_row(display, 1, "Progress is lost.");
    draw_footer(display, "No", "Yes", cursor);
}


static void draw_result_coarse(u8g2_t *display) {
    char buf[32];
    const powder_analysis_result_t *result = &powder_analysis_status.result;

    draw_title(display, "Result 1/3: Coarse");
    snprintf(buf, sizeof(buf), "Kp %.4g Kd %.3g", result->coarse_kp, result->coarse_kd);
    draw_row(display, 0, buf);
    // Coarse time and how far the throw went over the coarse stop point
    snprintf(buf, sizeof(buf), "%.1fs over %.2fgr", result->coarse_time_s, result->coarse_overshoot_gr);
    draw_row(display, 1, buf);
    coarse_hint_text(result, buf, sizeof(buf));
    draw_row(display, 2, buf);
    draw_footer(display, "", "Next", -1);
}


static void draw_result_fine(u8g2_t *display) {
    char buf[32];
    const powder_analysis_result_t *result = &powder_analysis_status.result;

    draw_title(display, "Result 2/3: Fine");
    snprintf(buf, sizeof(buf), "Kp %.4g Kd %.3g", result->fine_kp, result->fine_kd);
    draw_row(display, 0, buf);
    snprintf(buf, sizeof(buf), "%.1fs Hits %d/%d", result->total_time_s, result->verify_hits, POWDER_ANALYSIS_VERIFY_ROUND_THROWS);
    draw_row(display, 1, buf);
    result_reason_text(result, buf, sizeof(buf));
    draw_row(display, 2, buf);
    draw_footer(display, "", "Next", -1);
}


// D13: fine stop threshold recommendation from the fall after the stop (hint only, concept 4.10a)
static void draw_result_threshold(u8g2_t *display) {
    char buf[32];
    const powder_analysis_result_t *result = &powder_analysis_status.result;

    draw_title(display, "Result 3/3: Threshold");
    if (result->fall_count > 0) {
        snprintf(buf, sizeof(buf), "Rec. fine thr %.2f", result->recommended_fine_threshold);
        draw_row(display, 0, buf);
        snprintf(buf, sizeof(buf), "Lands ~ %+.3f gr", result->expected_final_offset_gr);
        draw_row(display, 1, buf);
    }
    else {
        draw_row(display, 0, "No fall data");
    }
    snprintf(buf, sizeof(buf), "Current %.2f", charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold);
    draw_row(display, 2, buf);
    draw_footer(display, "Save", "Discard", cursor);
}


static void draw_save_slot(u8g2_t *display) {
    const char *names[MAX_PROFILE_CNT];
    for (uint8_t idx = 0; idx < MAX_PROFILE_CNT; idx++) {
        names[idx] = profile_data.profiles[idx].name;
    }

    // Three rows fit, scroll so the cursor stays visible
    uint8_t first = (cursor >= 3) ? cursor - 2 : 0;

    draw_title(display, "Save to Slot");
    draw_list(display, names, MAX_PROFILE_CNT, first, 3);
    draw_footer(display, "Back", "Next", -1);
}


static void draw_save_confirm(u8g2_t *display) {
    char buf[32];
    draw_title(display, "Overwrite?");
    snprintf(buf, sizeof(buf), "%d: %s", powder_analysis_status.profile_idx, profile_data.profiles[powder_analysis_status.profile_idx].name);
    draw_row(display, 0, buf);
    snprintf(buf, sizeof(buf), "as %s", powder_analysis_status.profile_name);
    draw_row(display, 1, buf);
    draw_footer(display, "Back", "Save", cursor);
}


static void draw_activate(u8g2_t *display) {
    draw_title(display, "Saved");
    draw_row(display, 0, "Use this profile");
    draw_row(display, 1, "now?");
    draw_footer(display, "No", "Yes", cursor);
}


static void draw_test_done(u8g2_t *display) {
    char buf[32];
    const powder_analysis_trial_t *trial = &powder_analysis_status.last_trial;

    draw_title(display, "Test throw done");
    throw_result_text(trial, buf, sizeof(buf));
    draw_row(display, 0, buf);
    snprintf(buf, sizeof(buf), "Kp %.4g Kd %.3g", trial->kp, trial->kd);
    draw_row(display, 1, buf);
    snprintf(buf, sizeof(buf), "MinSpd %.3f rps", trial->fine_min_flow_speed_rps);
    draw_row(display, 2, buf);
    draw_footer(display, "Close", "", -1);
}


static void render_task(void *p) {
    u8g2_t *display = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        // The weight is shown on several screens and reported to the web interface
        powder_analysis_status.current_weight = scale_get_current_measurement();

        u8g2_ClearBuffer(display);

        switch (powder_analysis_status.state) {
            case POWDER_ANALYSIS_STATE_SETUP_WEIGHT:
                draw_setup_weight(display);
                break;
            case POWDER_ANALYSIS_STATE_SETUP_SHAPE:
                draw_title(display, "Powder Shape");
                draw_list(display, powder_shape_names, POWDER_SHAPE_COUNT, 0, 4);
                break;
            case POWDER_ANALYSIS_STATE_SETUP_GRAIN:
                draw_title(display, "Grain Size");
                draw_list(display, grain_size_names, GRAIN_SIZE_COUNT, 0, 3);
                break;
            case POWDER_ANALYSIS_STATE_PREPARE:
                draw_prepare(display);
                break;
            case POWDER_ANALYSIS_STATE_FLOODING:
                draw_flooding(display);
                break;
            case POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL:
            case POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN:
            case POWDER_ANALYSIS_STATE_WAIT_ZERO:
                draw_empty_pan(display);
                break;
            case POWDER_ANALYSIS_STATE_TRIAL:
            case POWDER_ANALYSIS_STATE_TEST_THROW:
                draw_trial(display);
                break;
            case POWDER_ANALYSIS_STATE_TEST_DONE:
                draw_test_done(display);
                break;
            case POWDER_ANALYSIS_STATE_PAUSED:
                draw_paused(display);
                break;
            case POWDER_ANALYSIS_STATE_ABORT_CONFIRM:
                draw_abort_confirm(display);
                break;
            case POWDER_ANALYSIS_STATE_RESULT_COARSE:
                draw_result_coarse(display);
                break;
            case POWDER_ANALYSIS_STATE_RESULT_FINE:
                draw_result_fine(display);
                break;
            case POWDER_ANALYSIS_STATE_RESULT_THRESHOLD:
                draw_result_threshold(display);
                break;
            case POWDER_ANALYSIS_STATE_SAVE_SLOT:
                draw_save_slot(display);
                break;
            case POWDER_ANALYSIS_STATE_SAVE_CONFIRM:
                draw_save_confirm(display);
                break;
            case POWDER_ANALYSIS_STATE_ACTIVATE:
                draw_activate(display);
                break;
            default:
                draw_title(display, "Powder Analysis");
                break;
        }

        u8g2_SendBuffer(display);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


// ---------------------------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------------------------
uint8_t powder_analysis_menu(void) {
    // If the display task is never created then we shall create one, otherwise we shall resume the task
    if (render_task_handler == NULL) {
        // The render task shall have lower priority than the current one. It needs more stack than
        // the minimum: several snprintf() with floats per frame, and a stack overflow would hang the
        // device (configCHECK_FOR_STACK_OVERFLOW).
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(render_task, "Powder Analysis Render Task", 512, NULL, current_task_priority - 1, &render_task_handler);
    }
    else {
        vTaskResume(render_task_handler);
    }
    ensure_command_queue();

    // Fresh start, the last charge weight is the proposed target
    memset(&powder_analysis_status, 0x0, sizeof(powder_analysis_status));
    powder_analysis_status.target_weight = charge_mode_config.target_charge_weight;
    weight_to_digits(powder_analysis_status.target_weight);

    // Not every scale driver can zero the scale (e.g. the generic driver has no force_zero)
    powder_analysis_status.zero_available = (scale_config.scale_handle != NULL &&
                                             scale_config.scale_handle->force_zero != NULL);

    // Started from the web? Then the setup values come with the command and the setup screens are
    // skipped. A test throw from the web brings all its values along as well.
    // Commands that were queued while the device was busy elsewhere are stale: only when a web
    // command caused this very entry its START / TEST_THROW is used.
    bool use_web_command = entered_from_web;
    entered_from_web = false;
    bool started_from_web = false;
    bool test_throw_from_web = false;
    powder_analysis_command_t command;
    powder_analysis_command_t test_command;
    while (xQueueReceive(command_queue, &command, 0) == pdTRUE) {
        if (!use_web_command) {
            continue;       // dropped
        }
        if (command.type == POWDER_ANALYSIS_CMD_START) {
            powder_analysis_status.target_weight = command.target_weight;
            powder_analysis_status.shape = command.shape;
            powder_analysis_status.grain_size = command.grain_size;
            started_from_web = true;
        }
        else if (command.type == POWDER_ANALYSIS_CMD_TEST_THROW) {
            test_command = command;
            test_throw_from_web = true;
        }
    }

    if (test_throw_from_web) {
        start_test_throw(&test_command, false); // the pan is on the scale, empty
    }
    else if (started_from_web && powder_analysis_status.target_weight > 0.0f) {
        go_to_state(POWDER_ANALYSIS_STATE_PREPARE, 0);
        set_prompt("Put pan on scale");
    }
    else {
        go_to_state(POWDER_ANALYSIS_STATE_SETUP_WEIGHT, 0);
        set_prompt("Enter target weight");
    }

    while (powder_analysis_status.state != POWDER_ANALYSIS_STATE_EXIT) {
        switch (powder_analysis_status.state) {
            case POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL:
                wait_for_pan_removal();
                break;
            case POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN:
                wait_for_pan_return();
                break;
            case POWDER_ANALYSIS_STATE_WAIT_ZERO:
                wait_for_zero();
                break;
            case POWDER_ANALYSIS_STATE_TRIAL:
                run_trial();
                break;
            case POWDER_ANALYSIS_STATE_TEST_THROW:
                run_test_throw();
                break;
            default:
                handle_screen_input();
                break;
        }
    }

    // Leave the module. The servo gate stays open on purpose (concept 4.3).
    stop_motors();
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);
    reset_led_colour();

    vTaskSuspend(render_task_handler);
    powder_analysis_status.has_result = false;      // the result cannot be saved anymore
    powder_analysis_status.test_mode = false;
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_IDLE;

    return 1;   // back to the main menu view
}


bool powder_analysis_post_command(const powder_analysis_command_t *command) {
    ensure_command_queue();
    if (xQueueSend(command_queue, command, 0) != pdTRUE) {
        return false;
    }

    // Wake the module loop if it is blocked waiting for a display button. While the module is
    // idle the caller enters it through the menu (exit_state + OVERRIDE_FROM_REST, like cleanup mode).
    if (powder_analysis_status.state != POWDER_ANALYSIS_STATE_IDLE && encoder_event_queue != NULL) {
        ButtonEncoderEvent_t button_event = OVERRIDE_FROM_REST;
        xQueueSend(encoder_event_queue, &button_event, 0);
    }
    return true;
}


// A consistent copy for the other tasks, see the note at the top
void powder_analysis_get_status(powder_analysis_status_t *status) {
    taskENTER_CRITICAL();
    *status = powder_analysis_status;
    taskEXIT_CRITICAL();
}


const char * powder_analysis_state_to_string(powder_analysis_state_t state) {
    switch (state) {
        case POWDER_ANALYSIS_STATE_IDLE:
            return "Idle";
        case POWDER_ANALYSIS_STATE_SETUP_WEIGHT:
            return "Target weight";
        case POWDER_ANALYSIS_STATE_SETUP_SHAPE:
            return "Powder shape";
        case POWDER_ANALYSIS_STATE_SETUP_GRAIN:
            return "Grain size";
        case POWDER_ANALYSIS_STATE_PREPARE:
            return "Prepare";
        case POWDER_ANALYSIS_STATE_FLOODING:
            return "Flooding";
        case POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL:
        case POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN:
        case POWDER_ANALYSIS_STATE_WAIT_ZERO:
            return "Empty pan";
        case POWDER_ANALYSIS_STATE_TRIAL:
            return "Throw";
        case POWDER_ANALYSIS_STATE_PAUSED:
            return "Paused";
        case POWDER_ANALYSIS_STATE_ABORT_CONFIRM:
            return "Abort?";
        case POWDER_ANALYSIS_STATE_RESULT_COARSE:
        case POWDER_ANALYSIS_STATE_RESULT_FINE:
        case POWDER_ANALYSIS_STATE_RESULT_THRESHOLD:
            return "Result";
        case POWDER_ANALYSIS_STATE_SAVE_SLOT:
        case POWDER_ANALYSIS_STATE_SAVE_CONFIRM:
            return "Save";
        case POWDER_ANALYSIS_STATE_ACTIVATE:
            return "Use profile?";
        case POWDER_ANALYSIS_STATE_EXIT:
            return "Exit";
        case POWDER_ANALYSIS_STATE_TEST_THROW:
            return "Test throw";
        case POWDER_ANALYSIS_STATE_TEST_DONE:
            return "Test throw done";
        default:
            return "Unknown";
    }
}


// ---------------------------------------------------------------------------------------------
// REST interface
// ---------------------------------------------------------------------------------------------
// Append formatted text to a JSON buffer. Keeps track of the position and never writes past the end.
static void json_append(char *buf, size_t buf_size, size_t *pos, const char *fmt, ...) {
    if (*pos >= buf_size - 1) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + *pos, buf_size - *pos, fmt, args);
    va_end(args);

    if (written > 0) {
        *pos += written;
        if (*pos > buf_size - 1) {
            *pos = buf_size - 1;    // output was cut off
        }
    }
}


// A weight as JSON number, or as the strings "nan" / "inf" when the scale gives no number
static void weight_to_json(float weight, char *buf, size_t len) {
    if (isnanf(weight)) {
        snprintf(buf, len, "\"nan\"");
    }
    else if (isinff(weight)) {
        snprintf(buf, len, "\"inf\"");
    }
    else {
        snprintf(buf, len, "%0.3f", weight);
    }
}


// Hand the finished response (header included) to the HTTP server
static bool json_finish(struct fs_file *file, const char *buf, size_t len) {
    file->data = buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return true;
}


// Finite and within [low, high]. NaN and inf fail every comparison, so they must be checked explicitly.
static bool in_range(float value, float low, float high) {
    return isfinite(value) && value >= low && value <= high;
}


// Check a command from the web and post it. Returns NULL when accepted, otherwise an error text.
static const char * post_web_command(const powder_analysis_command_t *command) {
    switch (command->type) {
        case POWDER_ANALYSIS_CMD_START:
            if (!in_range(command->target_weight, 0.01f, POWDER_ANALYSIS_MAX_TARGET_WEIGHT_GR)) {
                return "InvalidTargetWeight";
            }
            if ((int) command->shape < 0 || (int) command->shape >= POWDER_SHAPE_COUNT) {
                return "InvalidShape";
            }
            if ((int) command->grain_size < 0 || (int) command->grain_size >= GRAIN_SIZE_COUNT) {
                return "InvalidGrainSize";
            }
            break;

        case POWDER_ANALYSIS_CMD_SAVE:
            if (command->profile_idx >= MAX_PROFILE_CNT) {
                return "InvalidProfileIndex";      // also when pi was missing or not a number
            }
            // The name goes into JSON responses unescaped, so no quotes or backslashes
            if (strlen(command->profile_name) == 0 ||
                strchr(command->profile_name, '"') != NULL ||
                strchr(command->profile_name, '\\') != NULL) {
                return "InvalidProfileName";
            }
            break;

        case POWDER_ANALYSIS_CMD_TEST_THROW:
            if (!in_range(command->target_weight, 0.01f, POWDER_ANALYSIS_MAX_TARGET_WEIGHT_GR)) {
                return "InvalidTargetWeight";
            }
            if (!in_range(command->coarse_kp, 0.0f, TEST_THROW_MAX_GAIN) ||
                !in_range(command->coarse_kd, 0.0f, TEST_THROW_MAX_GAIN) ||
                !in_range(command->fine_kp, 0.0f, TEST_THROW_MAX_GAIN) ||
                !in_range(command->fine_kd, 0.0f, TEST_THROW_MAX_GAIN)) {
                return "InvalidGain";
            }
            if (!in_range(command->fine_min_flow_speed_rps, 0.001f, POWDER_ANALYSIS_FINE_MAX_FLOW_SPEED_RPS)) {
                return "InvalidMinSpeed";
            }
            // Only when nothing else is going on
            if (powder_analysis_status.state != POWDER_ANALYSIS_STATE_IDLE &&
                powder_analysis_status.state != POWDER_ANALYSIS_STATE_TEST_DONE) {
                return "AnalysisRunning";
            }
            break;

        case POWDER_ANALYSIS_CMD_NONE:
            return "InvalidCommand";

        default:
            if (command->type > POWDER_ANALYSIS_CMD_TEST_THROW) {
                return "InvalidCommand";
            }
            break;
    }

    if (!powder_analysis_post_command(command)) {
        return "QueueFull";
    }

    // Starting from the web while the module is idle: enter it the same way cleanup mode does.
    // The menu task leaves the menu and calls powder_analysis_menu(), which picks up the command.
    bool starts_the_module = (command->type == POWDER_ANALYSIS_CMD_START || command->type == POWDER_ANALYSIS_CMD_TEST_THROW);
    if (starts_the_module && powder_analysis_status.state == POWDER_ANALYSIS_STATE_IDLE) {
        entered_from_web = true;
        exit_state = APP_STATE_ENTER_POWDER_ANALYSIS;

        ButtonEncoderEvent_t button_event = OVERRIDE_FROM_REST;
        xQueueSend(encoder_event_queue, &button_event, portMAX_DELAY);
    }

    return NULL;
}


bool http_rest_powder_analysis_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // Control (all optional, a command is posted only when cmd is given):
    //   cmd (powder_analysis_command_type_t | int): command, see powder_analysis.h
    //   tw (float): target weight in gr, for cmd=START
    //   sh (powder_shape_t | int): powder shape, for cmd=START
    //   gs (grain_size_t | int): grain size, for cmd=START
    //   pi (int): profile slot, for cmd=SAVE
    //   pn (str): profile name (max PROFILE_NAME_MAX_LEN - 1 characters), for cmd=SAVE
    //   ckp, ckd, fkp, fkd (float): gains, fms (float): fine minimum speed rps, with tw, for cmd=TEST_THROW
    // Response:
    //   st (powder_analysis_state_t | int): state, sn (str): state name, ph (str): phase name
    //   pr (str): prompt for the user, tm (bool): test throw mode, fl (bool): flooding motors running,
    //   za (bool): scale can be zeroed
    //   tn (int): number of the running / next trial, tt (int): throw type (0 coarse only, 1 full;
    //   not to be confused with tt = total time inside lt and the trials list)
    //   ckp, ckd, fkp, fkd (float): gains of the running / next throw
    //   w (float, or the strings "nan" / "inf" when the scale gives no number): current weight
    //   el (float): elapsed seconds of the running (or last) throw
    //   tw (float), sh (int), gs (int): setup values; cs, fs (float): coarse / fine stop threshold
    //   sr (float): scale resolution in gr (config), for the threshold hint
    //   vr (int): verification round number (0 before verification), vrh (int): hits in that round, vrt (int): throws per round,
    //   fc (int): verification corrections so far, fcj (bool): the last throw gave its round up and a correction
    //   was applied, fck (bool): that correction lowered fine Kp (true) or the fine minimum speed (false)
    //   fcm (int): corrections at most, kpc, msc (int): percent a correction lowers fine Kp / the fine minimum
    //   speed by, fmd (float): fine minimum speed the analysis starts with (rps); all config values for the page texts
    //   tc (int): trials done so far (fetch /rest/powder_analysis_trials when it changes)
    //   tid (int): id of the last recorded throw (see /rest/powder_analysis_trace)
    //   lt (dict | null): last trial {n: number, w: weight, tt: total time, ev: evaluation name}
    //   res (dict | null): result, only once the analysis is finished:
    //       ckp, ckd, cmin, cmax, fkp, fkd, fmin, fmax (float): the profile values (Ki is always 0)
    //       vh (int): hits of the best round, fc (int): verification corrections (fkp / fmin carry the lowered values)
    //       ct, co, tt (float): coarse time, coarse overshoot, total time
    //       fn (int): throws with a measured fall after the stop, fm, fx (float): their median and maximum in gr,
    //       rft (float): recommended fine stop threshold from that median (hint only, nothing is changed),
    //       efo (float): expected final weight offset from the target with that threshold
    //       rs (str): reason text, ch (str): coarse hint text
    //   pi (int), pn (str): proposed save slot and name
    //   error (str): only when a command was rejected: InvalidCommand, InvalidTargetWeight, InvalidShape,
    //       InvalidGrainSize, InvalidProfileIndex, InvalidProfileName, ProfileNameTooLong, InvalidGain,
    //       InvalidMinSpeed, AnalysisRunning, QueueFull

    static char buf[1536];
    char weight_string[16];
    powder_analysis_command_t command;
    bool has_command = false;
    bool name_too_long = false;
    const char *error = NULL;

    // Control
    memset(&command, 0x0, sizeof(command));
    command.profile_idx = MAX_PROFILE_CNT;      // invalid until a valid pi arrives
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "cmd") == 0) {
            command.type = (powder_analysis_command_type_t) atoi(values[idx]);
            has_command = true;
        }
        else if (strcmp(params[idx], "tw") == 0) {
            command.target_weight = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "sh") == 0) {
            command.shape = (powder_shape_t) atoi(values[idx]);
        }
        else if (strcmp(params[idx], "gs") == 0) {
            command.grain_size = (grain_size_t) atoi(values[idx]);
        }
        else if (strcmp(params[idx], "pi") == 0) {
            char *end;
            long profile_idx = strtol(values[idx], &end, 10);
            if (*values[idx] != '\0' && *end == '\0' && profile_idx >= 0 && profile_idx < MAX_PROFILE_CNT) {
                command.profile_idx = (uint8_t) profile_idx;
            }
        }
        else if (strcmp(params[idx], "pn") == 0) {
            // Values arrive URL-decoded, so spaces are already spaces here
            if (strlen(values[idx]) >= PROFILE_NAME_MAX_LEN) {
                name_too_long = true;
            }
            snprintf(command.profile_name, sizeof(command.profile_name), "%s", values[idx]);
        }
        else if (strcmp(params[idx], "ckp") == 0) {
            command.coarse_kp = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "ckd") == 0) {
            command.coarse_kd = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fkp") == 0) {
            command.fine_kp = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fkd") == 0) {
            command.fine_kd = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fms") == 0) {
            command.fine_min_flow_speed_rps = strtof(values[idx], NULL);
        }
    }

    if (has_command) {
        error = name_too_long ? "ProfileNameTooLong" : post_web_command(&command);
    }

    // Response, built from a copy of the status (the analysis task keeps writing it)
    powder_analysis_status_t status_copy;
    powder_analysis_get_status(&status_copy);
    const powder_analysis_status_t *status = &status_copy;
    size_t pos = 0;

    // State, prompt, test mode
    json_append(buf, sizeof(buf), &pos,
                "%s{\"st\":%d,\"sn\":\"%s\",\"ph\":\"%s\",\"pr\":\"%s\",\"tm\":%s,\"fl\":%s,\"za\":%s,",
                http_json_header,
                (int) status->state,
                powder_analysis_state_to_string(status->state),
                powder_analysis_phase_to_string(status->phase),
                status->prompt,
                boolean_to_string(status->test_mode),
                boolean_to_string(status->flooding_running),
                boolean_to_string(status->zero_available));

    // Running (or last) throw. The scale is read directly: status->current_weight is only
    // refreshed by the display render task, and the page also polls while the module is idle.
    weight_to_json(scale_get_current_measurement(), weight_string, sizeof(weight_string));
    json_append(buf, sizeof(buf), &pos,
                "\"tn\":%d,\"tt\":%d,\"ckp\":%0.4f,\"ckd\":%0.4f,\"fkp\":%0.4f,\"fkd\":%0.4f,\"w\":%s,\"el\":%0.1f,",
                status->trial.trial_number,
                (int) status->trial.throw_type,
                status->trial.coarse_kp, status->trial.coarse_kd, status->trial.fine_kp, status->trial.fine_kd,
                weight_string,
                status->elapsed_s);

    // Setup values, stop thresholds and scale resolution
    json_append(buf, sizeof(buf), &pos,
                "\"tw\":%0.2f,\"sh\":%d,\"gs\":%d,\"cs\":%0.3f,\"fs\":%0.3f,\"sr\":%0.3f,",
                status->target_weight,
                (int) status->shape,
                (int) status->grain_size,
                charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold,
                charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold,
                POWDER_ANALYSIS_SCALE_RESOLUTION_GR);

    // Verification progress and the correction rules of the config
    json_append(buf, sizeof(buf), &pos,
                "\"vr\":%d,\"vrh\":%d,\"vrt\":%d,\"fc\":%d,\"fcj\":%s,\"fck\":%s,"
                "\"fcm\":%d,\"kpc\":%d,\"msc\":%d,\"fmd\":%0.3f,",
                status->verify_round,
                status->verify_round_hits,
                POWDER_ANALYSIS_VERIFY_ROUND_THROWS,
                status->fine_corrections,
                boolean_to_string(status->correction_just_applied),
                boolean_to_string(status->correction_lowered_kp),
                POWDER_ANALYSIS_MAX_FINE_CORRECTIONS,
                correction_percent(POWDER_ANALYSIS_FINE_KP_CORRECTION_FACTOR),
                correction_percent(POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR),
                POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS);

    // Trial protocol and recording
    json_append(buf, sizeof(buf), &pos, "\"tc\":%d,\"tid\":%u,", tuner.trial_count, (unsigned) trace.id);

    // Last trial
    if (status->has_last_trial) {
        const powder_analysis_trial_t *trial = &status->last_trial;
        json_append(buf, sizeof(buf), &pos,
                    "\"lt\":{\"n\":%d,\"w\":%0.3f,\"tt\":%0.1f,\"ev\":\"%s\"},",
                    trial->number, trial->weight, trial->total_time_s,
                    powder_analysis_evaluation_to_string(trial->evaluation));
    }
    else {
        json_append(buf, sizeof(buf), &pos, "\"lt\":null,");
    }

    // Result
    if (status->has_result) {
        const powder_analysis_result_t *result = &status->result;
        char reason[32];
        char coarse_hint[32];
        result_reason_text(result, reason, sizeof(reason));
        coarse_hint_text(result, coarse_hint, sizeof(coarse_hint));
        json_append(buf, sizeof(buf), &pos,
                    "\"res\":{\"ckp\":%0.4f,\"ckd\":%0.4f,\"cmin\":%0.3f,\"cmax\":%0.3f,"
                    "\"fkp\":%0.4f,\"fkd\":%0.4f,\"fmin\":%0.3f,\"fmax\":%0.3f,"
                    "\"vh\":%d,\"fc\":%d,"
                    "\"ct\":%0.1f,\"co\":%0.3f,\"tt\":%0.1f,"
                    "\"fn\":%d,\"fm\":%0.3f,\"fx\":%0.3f,\"rft\":%0.2f,\"efo\":%0.3f,\"rs\":\"%s\",\"ch\":\"%s\"},",
                    result->coarse_kp, result->coarse_kd, result->coarse_min_flow_speed_rps, result->coarse_max_flow_speed_rps,
                    result->fine_kp, result->fine_kd, result->fine_min_flow_speed_rps, result->fine_max_flow_speed_rps,
                    result->verify_hits, result->fine_corrections,
                    result->coarse_time_s, result->coarse_overshoot_gr, result->total_time_s,
                    result->fall_count, result->fall_median_gr, result->fall_max_gr, result->recommended_fine_threshold,
                    result->expected_final_offset_gr,
                    reason, coarse_hint);
    }
    else {
        json_append(buf, sizeof(buf), &pos, "\"res\":null,");
    }

    json_append(buf, sizeof(buf), &pos, "\"pi\":%d,\"pn\":\"%s\"", status->profile_idx, status->profile_name);

    if (error != NULL) {
        json_append(buf, sizeof(buf), &pos, ",\"error\":\"%s\"", error);
    }
    json_append(buf, sizeof(buf), &pos, "}");
    return json_finish(file, buf, pos);
}


bool http_rest_powder_analysis_trials(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // Response (no parameters):
    //   tc (int): number of trials
    //   t (list): one entry per trial {n: number, ph: phase name, kp, kd: gains of the trickler that
    //             phase tunes (coarse gains in the coarse phases, fine gains otherwise), w: weight,
    //             ct: coarse time, tt: total time, ev: evaluation name, fms: fine minimum speed rps,
    //             fa: fall after the stop in gr (settled weight - weight at the fine stop), null if not measured}

    // One entry takes about 125 characters, the buffer holds all POWDER_ANALYSIS_MAX_TRIALS of them
    static char buf[64 + POWDER_ANALYSIS_MAX_TRIALS * 140];
    size_t pos = 0;

    // The count is taken once: entries below it are complete (see the note at the top)
    int trial_count = tuner.trial_count;
    json_append(buf, sizeof(buf), &pos, "%s{\"tc\":%d,\"t\":[", http_json_header, trial_count);

    for (int idx = 0; idx < trial_count; idx += 1) {
        const powder_analysis_trial_t *trial = &tuner.trials[idx];
        char fall_string[16];
        if (trial->has_fall) {
            snprintf(fall_string, sizeof(fall_string), "%0.3f", trial->fall_gr);
        }
        else {
            snprintf(fall_string, sizeof(fall_string), "null");
        }
        json_append(buf, sizeof(buf), &pos,
                    "%s{\"n\":%d,\"ph\":\"%s\",\"kp\":%0.4f,\"kd\":%0.4f,\"w\":%0.3f,\"ct\":%0.1f,\"tt\":%0.1f,\"ev\":\"%s\",\"fms\":%0.3f,\"fa\":%s}",
                    idx > 0 ? "," : "",
                    trial->number,
                    powder_analysis_phase_to_string(trial->phase),
                    trial->kp, trial->kd, trial->weight, trial->coarse_time_s, trial->total_time_s,
                    powder_analysis_evaluation_to_string(trial->evaluation),
                    trial->fine_min_flow_speed_rps,
                    fall_string);
    }

    json_append(buf, sizeof(buf), &pos, "]}");
    return json_finish(file, buf, pos);
}


// Samples per response of the trace endpoint: keeps the static buffer small, the page fetches in chunks
#define TRACE_CHUNK_SAMPLES 40

bool http_rest_powder_analysis_trace(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // Control:
    //   from (int): index of the first sample to return (default 0)
    // Response:
    //   id (int): id of the recorded throw (changes with every finished throw), n (int): samples recorded
    //   p (dict): the values of the throw {tw, ckp, ckd, fkp, fkd, fms}, fw: settled final weight (null if not
    //             known), k: kind ("test" or the phase name of the analysis throw)
    //   s (list): up to TRACE_CHUNK_SAMPLES samples starting at `from`:
    //             [t_s, weight, fine_p, fine_d, fine_speed, coarse_p, coarse_d, coarse_speed]

    // About 100 characters per sample row at most
    static char buf[256 + TRACE_CHUNK_SAMPLES * 110];
    size_t pos = 0;
    int from = 0;

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "from") == 0) {
            from = atoi(values[idx]);
        }
    }
    if (from < 0) {
        from = 0;
    }

    char final_weight_string[16];
    if (isnanf(trace.final_weight)) {
        snprintf(final_weight_string, sizeof(final_weight_string), "null");
    }
    else {
        snprintf(final_weight_string, sizeof(final_weight_string), "%0.3f", trace.final_weight);
    }

    json_append(buf, sizeof(buf), &pos,
                "%s{\"id\":%u,\"n\":%u,"
                "\"p\":{\"tw\":%0.2f,\"ckp\":%0.4f,\"ckd\":%0.4f,\"fkp\":%0.4f,\"fkd\":%0.4f,\"fms\":%0.3f,\"fw\":%s,\"k\":\"%s\"},\"s\":[",
                http_json_header,
                (unsigned) trace.id,
                (unsigned) trace.count,
                trace.target_weight,
                trace.params.coarse_kp, trace.params.coarse_kd, trace.params.fine_kp, trace.params.fine_kd,
                trace.params.fine_min_flow_speed_rps,
                final_weight_string,
                trace.test_throw ? "test" : powder_analysis_phase_to_string(trace.params.phase));

    for (int idx = from; idx < trace.count && idx < from + TRACE_CHUNK_SAMPLES; idx += 1) {
        const powder_analysis_trace_sample_t *sample = &trace.samples[idx];
        json_append(buf, sizeof(buf), &pos,
                    "%s[%0.2f,%0.3f,%0.4f,%0.4f,%0.3f,%0.4f,%0.4f,%0.3f]",
                    idx > from ? "," : "",
                    sample->t_s, sample->weight,
                    sample->fine_p, sample->fine_d, sample->fine_speed,
                    sample->coarse_p, sample->coarse_d, sample->coarse_speed);
    }

    json_append(buf, sizeof(buf), &pos, "]}");
    return json_finish(file, buf, pos);
}
