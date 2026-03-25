#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include <u8g2.h>
#include <math.h>

#include "app.h"
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
#include "ai_tuning.h"


uint8_t charge_weight_digits[] = {0, 0, 0, 0, 0};

charge_mode_config_t charge_mode_config;

// Scale related
extern scale_config_t scale_config;
extern servo_gate_t servo_gate;


const eeprom_charge_mode_data_t default_charge_mode_data = {
    .charge_mode_data_rev = 0,

    .coarse_stop_threshold = 4,
    .fine_stop_threshold = 0.03,

    .set_point_sd_margin = 0.02,
    .set_point_mean_margin = 0.02,
    .coarse_stop_gate_ratio = 0,   // NEW
    .decimal_places = DP_2,

    // Precharges
    .precharge_enable = false,
    .precharge_time_ms = 1000,
    .precharge_speed_rps = 2,

    // AI tuning time targets (defaults: 7000ms coarse pre-charge, 15000ms total)
    .coarse_time_target_ms = 7000,
    .total_time_target_ms = 15000,

    // ML data collection disabled by default
    .ml_data_collection_enabled = false,

    // Auto zero disabled by default
    .auto_zero_on_cup_return = false,

    // Pulse mode defaults (disabled by default)
    .pulse_mode_enabled = false,
    .pulse_threshold = 0.5f,        // Start pulsing when within 0.5 grains (range: 0.3-1.0)
    .pulse_duration_ms = 30,        // 30ms motor burst
    .pulse_wait_ms = 150,           // 150ms wait for scale

    // Scale stabilization defaults
    .stabilization_enabled = false,     // adaptive by default
    .stabilization_time_ms = 2000,      // 2s fixed wait when enabled

    // LED related
    .neopixel_normal_charge_colour = RGB_COLOUR_GREEN,        // green
    .neopixel_under_charge_colour = RGB_COLOUR_YELLOW,        // yellow
    .neopixel_over_charge_colour = RGB_COLOUR_RED,            // red
    .neopixel_not_ready_colour = RGB_COLOUR_BLUE,             // blue
};

// Configures
TaskHandle_t scale_measurement_render_task_handler = NULL;
static char title_string[30];

static TickType_t charge_start_tick = 0;
static float last_charge_elapsed_seconds = 0.0f;

// Deferred ML recording (set in charge loop, written during cup removal to avoid flash blocking)
static bool ml_record_pending = false;
static float ml_coarse_time_ms = 0.0f;
static float ml_fine_time_ms = 0.0f;

// Deferred AI tuning recording (same pattern - defer to cup removal for settled scale reading)
static bool ai_record_pending = false;
static ai_drop_telemetry_t ai_pending_telemetry;
static ai_motor_mode_t ai_pending_motor_mode;

// Menu system
extern AppState_t exit_state;
extern QueueHandle_t encoder_event_queue;
extern neopixel_led_config_t neopixel_led_config;


// Definitions
typedef enum {
    CHARGE_MODE_EVENT_NO_EVENT = (1 << 0),
    CHARGE_MODE_EVENT_UNDER_CHARGE = (1 << 1),
    CHARGE_MODE_EVENT_OVER_CHARGE = (1 << 2),
} ChargeModeEventBit_t;


static void format_elapsed_time(char *buffer, size_t len, TickType_t start_tick) {
    TickType_t now = xTaskGetTickCount();
    uint32_t elapsed_ticks = now - start_tick;

    // Tick to milliseconds
    float elapsed_seconds = (float)(elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    snprintf(buffer, len, "%.2f s", elapsed_seconds);
}


void scale_measurement_render_task(void *p) {
    char current_weight_string[WEIGHT_STRING_LEN];
    char time_buffer[16];

    u8g2_t *display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(display_handler);

        // Set font for title and timer
        u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);

        // Format the timer string based on current state
        if (charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_COMPLETE) {
            format_elapsed_time(time_buffer, sizeof(time_buffer), charge_start_tick);
        } else if (charge_mode_config.charge_mode_state == CHARGE_MODE_STABILIZING ||
                   charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_CUP_REMOVAL ||
                   charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_CUP_RETURN ||
                   charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_ZERO) {
            snprintf(time_buffer, sizeof(time_buffer), "%.2f s", last_charge_elapsed_seconds);
        } else {
            snprintf(time_buffer, sizeof(time_buffer), "--.- s");
        }

        // Calculate x positions
        uint8_t screen_width = u8g2_GetDisplayWidth(display_handler);
        uint8_t time_width = u8g2_GetStrWidth(display_handler, time_buffer);

        // Draw title on left
        u8g2_DrawStr(display_handler, 5, 10, title_string);

        // Draw timer on right edge
        u8g2_DrawStr(display_handler, screen_width - time_width - 5, 10, time_buffer);  // 5 px padding from edge

        // Draw line under title
        u8g2_DrawHLine(display_handler, 0, 13, screen_width);

        // Current weight (only show values > -1.0)
        memset(current_weight_string, 0x0, sizeof(current_weight_string));
        float scale_measurement = scale_get_current_measurement();
        if (scale_measurement > -1.0) {
            float_to_string(current_weight_string, scale_measurement, charge_mode_config.eeprom_charge_mode_data.decimal_places);
        } else {
            strcpy(current_weight_string, "---");
        }

        // Draw current weight value
        u8g2_SetFont(display_handler, u8g2_font_profont22_tf);
        u8g2_DrawStr(display_handler, 26, 35, current_weight_string);

        // Draw profile name
        profile_t *current_profile = profile_get_selected();
        u8g2_SetFont(display_handler, u8g2_font_helvR08_tr);
        u8g2_DrawStr(display_handler, 5, 61, current_profile->name);

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


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

    // Stop condition: 10 stable measurements in 200ms apart (2 seconds minimum)
    while (true) {
        TickType_t last_measurement_tick = xTaskGetTickCount();

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }
        else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
            scale_config.scale_handle->force_zero();
        }

        // Perform measurement (max delay 300 seconds   )
        float current_measurement;
        if (scale_block_wait_for_next_measurement(300, &current_measurement)){
            data_buffer.enqueue(current_measurement);
        }

        // Generate stop condition
        if (data_buffer.getCounter() >= 10){
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin && 
                abs(data_buffer.getMean()) < charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin) {
                break;
            }
        }

        // Wait for minimum 300 ms (but can skip if previously wait already)
        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(300));
    }

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_COMPLETE;
}

void charge_mode_wait_for_complete() {

    charge_start_tick = xTaskGetTickCount();

    // AI tuning: determine motor mode and get next params for this drop
    ai_motor_mode_t ai_motor_mode = ai_tuning_get_motor_mode();
    bool ai_active = ai_tuning_is_active();
    float ai_coarse_kp = 0.0f, ai_coarse_kd = 0.0f;
    float ai_fine_kp = 0.0f, ai_fine_kd = 0.0f;
    if (ai_active) {
        ai_tuning_get_next_params(&ai_coarse_kp, &ai_coarse_kd, &ai_fine_kp, &ai_fine_kd);
    }

    // Set colour to under charge
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour,
        charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour,
        true
    );

    // If the servo gate is used then it has to be opened
    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_OPEN, false);
    }

    // Update current status
    char target_weight_string[WEIGHT_STRING_LEN];
    float_to_string(target_weight_string, charge_mode_config.target_charge_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);
    snprintf(title_string, sizeof(title_string), "Target: %s", target_weight_string);

    // Read trickling parameter from the current profile
    profile_t * current_profile = profile_get_selected();

    // Use AI-tuned params if active, otherwise use profile params
    float coarse_kp_used = ai_active ? ai_coarse_kp : current_profile->coarse_kp;
    float coarse_kd_used = ai_active ? ai_coarse_kd : current_profile->coarse_kd;
    float fine_kp_used   = ai_active ? ai_fine_kp   : current_profile->fine_kp;
    float fine_kd_used   = ai_active ? ai_fine_kd   : current_profile->fine_kd;

    // Find the minimum of max speed from the motor and the profile
    float coarse_trickler_max_speed = fmin(get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           current_profile->coarse_max_flow_speed_rps);
    float coarse_trickler_min_speed = fmax(get_motor_min_speed(SELECT_COARSE_TRICKLER_MOTOR),
                                           current_profile->coarse_min_flow_speed_rps);
    float fine_trickler_max_speed = fmin(get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         current_profile->fine_max_flow_speed_rps);
    float fine_trickler_min_speed = fmax(get_motor_min_speed(SELECT_FINE_TRICKLER_MOTOR),
                                         current_profile->fine_min_flow_speed_rps);

    // Phase 2 (FINE_ONLY): run a coarse pre-charge using tuned coarse PID from Phase 1
    TickType_t coarse_stop_tick = 0;
    if (ai_active && ai_motor_mode == AI_MOTOR_MODE_FINE_ONLY) {
        ai_tuning_session_t *session = ai_tuning_get_session();
        float tuned_coarse_kp = session->recommended_coarse_kp;
        float tuned_coarse_kd = session->recommended_coarse_kd;

        float precharge_target = charge_mode_config.target_charge_weight -
                                 charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;

        float precharge_integral = 0.0f;
        float precharge_last_error = 0.0f;
        TickType_t precharge_last_tick = xTaskGetTickCount();

        int precharge_scale_fail_count = 0;
        while (true) {
            float current_weight;
            if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
                precharge_scale_fail_count++;
                if (precharge_scale_fail_count >= 10) {
                    // Scale disconnected for ~2 seconds - emergency stop
                    motor_set_speed(SELECT_BOTH_MOTOR, 0);
                    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
                    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);
                    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
                        servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, false);
                    }
                    charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                    return;
                }
                continue;
            }
            precharge_scale_fail_count = 0;

            float precharge_error = precharge_target - current_weight;

            if (precharge_error < charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold) {
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
                // Settle before fine PID starts
                vTaskDelay(pdMS_TO_TICKS(1000));
                coarse_stop_tick = xTaskGetTickCount();
                break;
            }

            TickType_t now = xTaskGetTickCount();
            float dt_ms = (float)((now - precharge_last_tick) * portTICK_PERIOD_MS);
            precharge_integral += precharge_error;
            // Anti-windup clamp
            precharge_integral = fmaxf(-50.0f, fminf(precharge_integral, 50.0f));
            float derivative = (dt_ms > 0.0f) ? (precharge_error - precharge_last_error) / dt_ms : 0.0f;

            float pid_output = tuned_coarse_kp * precharge_error
                             + current_profile->coarse_ki * precharge_integral
                             + tuned_coarse_kd * derivative;
            if (pid_output <= 0.0f) {
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            } else {
                float spd = fmax(coarse_trickler_min_speed, fmin(pid_output, coarse_trickler_max_speed));
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, spd);
            }

            precharge_last_tick = now;
            precharge_last_error = precharge_error;

            // User abort
            ButtonEncoderEvent_t btn = button_wait_for_input(false);
            if (btn == BUTTON_RST_PRESSED) {
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                if (ai_active) ai_tuning_cancel();
                return;
            }
        }
    }

    float integral = 0.0f;
    float last_error = 0.0f;

    TickType_t last_sample_tick = xTaskGetTickCount();
    TickType_t current_sample_tick = last_sample_tick;

    // In FINE_ONLY mode coarse trickler was already handled by pre-charge above
    bool should_coarse_trickler_move = (ai_motor_mode != AI_MOTOR_MODE_FINE_ONLY);

    int scale_fail_count = 0;
    while (true) {
        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            if (ai_active) {
                ai_tuning_cancel();
            }
            return;
        }

        // Run the PID controlled loop to start charging
        // Perform the measurement
        float current_weight;
        if (!scale_block_wait_for_next_measurement(200, &current_weight)) {
            // If no measurement within 200ms then poll the button and retry
            scale_fail_count++;
            if (scale_fail_count >= 10) {
                // Scale disconnected for ~2 seconds - emergency stop
                motor_set_speed(SELECT_BOTH_MOTOR, 0);
                motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
                motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);
                if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
                    servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, false);
                }
                if (ai_active) ai_tuning_cancel();
                charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                return;
            }
            continue;
        }
        scale_fail_count = 0;
        current_sample_tick = xTaskGetTickCount();

        float error = charge_mode_config.target_charge_weight - current_weight;

        // Stop condition - Phase 1 (COARSE_ONLY) exits at coarse threshold; others at fine threshold
        if (ai_motor_mode == AI_MOTOR_MODE_COARSE_ONLY &&
            error <= charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            coarse_stop_tick = xTaskGetTickCount();
            break;
        }

        if (error <= charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold) {
            // Stop all motors
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            break;
        }

        // Coarse trickler move condition
        else if (error < charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold &&
                 should_coarse_trickler_move) {

            should_coarse_trickler_move = false;
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            coarse_stop_tick = xTaskGetTickCount();

            // When the coarse trickler stops, move the servo gate to a configured ratio
            // Ratio convention: 0.0 = open, 1.0 = close
            if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
                float r = charge_mode_config.eeprom_charge_mode_data.coarse_stop_gate_ratio;
                servo_gate_set_ratio(r, false); // don't block the charge loop
            }
        }

        // Update PID variables
        float elapse_time_ms = (current_sample_tick - last_sample_tick) / portTICK_RATE_MS;
        integral += error;
        float derivative = (error - last_error) / elapse_time_ms;

        float new_p, new_i, new_d, new_speed;

        // Update fine trickler speed (skip if COARSE_ONLY mode)
        if (ai_motor_mode != AI_MOTOR_MODE_COARSE_ONLY) {
            bool use_pulse = charge_mode_config.eeprom_charge_mode_data.pulse_mode_enabled &&
                             error < charge_mode_config.eeprom_charge_mode_data.pulse_threshold &&
                             error > charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;

            if (use_pulse) {
                // Pulse mode: short burst then wait for scale to settle
                float pulse_speed = fmax(fine_trickler_min_speed, fine_trickler_max_speed * 0.3f);
                motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, pulse_speed);
                vTaskDelay(pdMS_TO_TICKS(charge_mode_config.eeprom_charge_mode_data.pulse_duration_ms));
                motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
                vTaskDelay(pdMS_TO_TICKS(charge_mode_config.eeprom_charge_mode_data.pulse_wait_ms));
            } else {
                new_p = fine_kp_used * error;
                new_i = current_profile->fine_ki * integral;
                new_d = fine_kd_used * derivative;
                float fine_pid = new_p + new_i + new_d;
                // Phase 2 FINE_ONLY: stop motor when PID <= 0 (don't force min speed past target)
                if (ai_motor_mode == AI_MOTOR_MODE_FINE_ONLY && fine_pid <= 0.0f) {
                    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
                } else {
                    new_speed = fmax(fine_trickler_min_speed, fmin(fine_pid, fine_trickler_max_speed));
                    motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, new_speed);
                }
            }
        }

        // Update coarse trickler speed
        if (should_coarse_trickler_move) {
            new_p = coarse_kp_used * error;
            new_i = current_profile->coarse_ki * integral;
            new_d = coarse_kd_used * derivative;
            float coarse_pid = new_p + new_i + new_d;
            // Phase 1 COARSE_ONLY: stop motor when PID <= 0 (don't force min speed past target)
            if (ai_motor_mode == AI_MOTOR_MODE_COARSE_ONLY && coarse_pid <= 0.0f) {
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
            } else {
                new_speed = fmax(coarse_trickler_min_speed, fmin(coarse_pid, coarse_trickler_max_speed));
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, new_speed);
            }
        }

        // Record state
        last_sample_tick = current_sample_tick;
        last_error = error;
    }

    // Stop the timer
    TickType_t end_tick = xTaskGetTickCount();
    TickType_t elapsed_ticks = end_tick - charge_start_tick;
    last_charge_elapsed_seconds = (float)(elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    // Record AI telemetry or background ML data
    float total_ms = (float)(elapsed_ticks * portTICK_PERIOD_MS);
    float coarse_ms = coarse_stop_tick ? (float)((coarse_stop_tick - charge_start_tick) * portTICK_PERIOD_MS) : 0.0f;
    float fine_ms = total_ms - coarse_ms;

    if (ai_active) {
        // Defer AI telemetry to cup_removal (1s settle) for accurate weight reading, same as ML
        ai_pending_telemetry.drop_number    = ai_tuning_get_session()->drops_completed;
        ai_pending_telemetry.coarse_time_ms = coarse_ms;
        ai_pending_telemetry.fine_time_ms   = fine_ms;
        ai_pending_telemetry.total_time_ms  = total_ms;
        ai_pending_telemetry.coarse_kp_used = coarse_kp_used;
        ai_pending_telemetry.coarse_kd_used = coarse_kd_used;
        ai_pending_telemetry.fine_kp_used   = fine_kp_used;
        ai_pending_telemetry.fine_kd_used   = fine_kd_used;
        ai_pending_telemetry.target_weight  = charge_mode_config.target_charge_weight;
        ai_pending_motor_mode               = ai_motor_mode;
        ai_record_pending                   = true;
    }
    else if (charge_mode_config.eeprom_charge_mode_data.ml_data_collection_enabled) {
        // Defer ML recording to cup removal phase — flash_safe_execute is too slow
        // to call here without risking timeout while WiFi stack is active
        ml_record_pending = true;
        ml_coarse_time_ms = coarse_ms;
        ml_fine_time_ms = fine_ms;
    }

    // Close the gate if the servo gate is present
    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_CLOSED, true);
    }

    // Precharge
    if (charge_mode_config.eeprom_charge_mode_data.precharge_enable &&
        servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
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

    charge_mode_config.charge_mode_state = CHARGE_MODE_STABILIZING;
}

void charge_mode_stabilize() {
    snprintf(title_string, sizeof(title_string), "Stabilizing...");

    // Wait for scale to stabilize after motors stopped
    if (charge_mode_config.eeprom_charge_mode_data.stabilization_enabled) {
        // Fixed configured wait
        vTaskDelay(pdMS_TO_TICKS(charge_mode_config.eeprom_charge_mode_data.stabilization_time_ms));
    } else {
        // Adaptive: collect samples until SD < set_point_sd_margin or 3s timeout
        FloatRingBuffer stab_buffer(5);
        TickType_t stab_start = xTaskGetTickCount();
        const TickType_t stab_timeout = pdMS_TO_TICKS(3000);
        while ((xTaskGetTickCount() - stab_start) < stab_timeout) {
            float stab_weight;
            if (scale_block_wait_for_next_measurement(200, &stab_weight)) {
                stab_buffer.enqueue(stab_weight);
                if (stab_buffer.getCounter() >= 5 &&
                    stab_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin) {
                    break;
                }
            }
        }
    }

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_CUP_REMOVAL;
}

void charge_mode_wait_for_cup_removal() {
    // Update current status
    snprintf(title_string, sizeof(title_string), "Remove Cup");

    FloatRingBuffer data_buffer(5);

    // Take current measurement (settled reading used for ML overthrow too)
    float current_measurement = scale_get_current_measurement();
    float error = charge_mode_config.target_charge_weight - current_measurement;

    // Deferred AI tuning recording — use settled measurement for accurate weight
    if (ai_record_pending) {
        ai_record_pending = false;
        // Phase 1: overthrow relative to coarse stop point; Phase 2+: relative to full target
        float ai_effective_target;
        if (ai_pending_motor_mode == AI_MOTOR_MODE_COARSE_ONLY) {
            ai_effective_target = ai_pending_telemetry.target_weight -
                                  charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;
        } else {
            ai_effective_target = ai_pending_telemetry.target_weight;
        }
        float ai_overthrow = current_measurement - ai_effective_target;
        ai_pending_telemetry.final_weight      = current_measurement;
        ai_pending_telemetry.overthrow         = ai_overthrow;
        ai_pending_telemetry.overthrow_percent = (ai_effective_target > 0.0f)
                                                 ? (ai_overthrow / ai_effective_target) * 100.0f
                                                 : 0.0f;
        ai_tuning_record_drop(&ai_pending_telemetry);
    }

    // Deferred ML recording — use settled measurement taken after 1s delay
    if (ml_record_pending) {
        ml_record_pending = false;
        profile_t* ml_profile = profile_get_selected();
        float ml_overthrow = current_measurement - charge_mode_config.target_charge_weight;
        ai_tuning_record_charge(
            (uint8_t) profile_get_selected_idx(),
            ml_profile->coarse_kp, ml_profile->coarse_kd,
            ml_profile->fine_kp, ml_profile->fine_kd,
            ml_overthrow,
            ml_coarse_time_ms, ml_fine_time_ms
        );
    }

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

    // Stop condition: 5 stable measurements in 300ms apart (1.5 seconds minimum)
    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
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

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
            return;
        }
        else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
            scale_config.scale_handle->force_zero();
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

    // Auto zero scale if enabled
    if (charge_mode_config.eeprom_charge_mode_data.auto_zero_on_cup_return) {
        scale_config.scale_handle->force_zero();
    }

    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_ZERO;
}


uint8_t charge_mode_menu(bool charge_mode_skip_user_input) {
    // Create target weight, if the charge mode weight is built by charge_weight_digits
    if (!charge_mode_skip_user_input) {
        switch (charge_mode_config.eeprom_charge_mode_data.decimal_places) {
            case DP_2:
                charge_mode_config.target_charge_weight = charge_weight_digits[4] * 100 + \
                                                charge_weight_digits[3] * 10 + \
                                                charge_weight_digits[2] * 1 + \
                                                charge_weight_digits[1] * 0.1 + \
                                                charge_weight_digits[0] * 0.01;
                break;
            case DP_3:
                charge_mode_config.target_charge_weight = charge_weight_digits[4] * 10 + \
                                                charge_weight_digits[3] * 1 + \
                                                charge_weight_digits[2] * 0.1 + \
                                                charge_weight_digits[1] * 0.01 + \
                                                charge_weight_digits[0] * 0.001;
                break;
            default:
                charge_mode_config.target_charge_weight = 0;
                break;
        }
    }

    // If the display task is never created then we shall create one, otherwise we shall resume the task
    if (scale_measurement_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(scale_measurement_render_task, "Scale Measurement Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &scale_measurement_render_task_handler);
    }
    else {
        vTaskResume(scale_measurement_render_task_handler);
    }

    // Enable motor on entering the charge mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);
    
    charge_mode_config.charge_mode_state = CHARGE_MODE_WAIT_FOR_ZERO;

    bool quit = false;
    while (quit == false) {
        switch (charge_mode_config.charge_mode_state) {
            case CHARGE_MODE_WAIT_FOR_ZERO:
                charge_mode_wait_for_zero();
                break;
            case CHARGE_MODE_WAIT_FOR_COMPLETE:
                charge_mode_wait_for_complete();
                break;
            case CHARGE_MODE_STABILIZING:
                charge_mode_stabilize();
                break;
            case CHARGE_MODE_WAIT_FOR_CUP_REMOVAL:
                charge_mode_wait_for_cup_removal();
                // If AI tuning just completed, exit after the user removes the cup
                if (ai_tuning_is_complete()) {
                    charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
                }
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

    // vTaskDelete(scale_measurement_render_handler);
    vTaskSuspend(scale_measurement_render_task_handler);

    // Diable motors on exiting the mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);

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

    return true;
}


bool charge_mode_config_save(void) {
    bool is_ok = save_config(EEPROM_CHARGE_MODE_BASE_ADDR, &charge_mode_config.eeprom_charge_mode_data, sizeof(eeprom_charge_mode_data_t));
    return is_ok;
}



bool http_rest_charge_mode_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // c1 (str): neopixel_normal_charge_colour
    // c2 (str): neopixel_under_charge_colour
    // c3 (str): neopixel_over_charge_colour
    // c4 (str): neopixel_not_ready_colour

    // c5 (float): coarse_stop_threshold
    // c6 (float): fine_stop_threshold
    // c7 (float): set_point_sd_margin
    // c8 (float): set_point_mean_margin
    // c9 (int): decimal point enum
    // c10 (bool): precharge_enable
    // c11 (int): precharge_time_ms
    // c12 (float): precharge_speed_rps
    // c13 (float): coarse_stop_gate_ratio
    // c14 (uint32): coarse_time_target_ms (AI tuning coarse pre-charge duration)
    // c15 (uint32): total_time_target_ms  (AI tuning total time target)
    // c16 (bool): ml_data_collection_enabled
    // ee (bool): save to eeprom

    static char charge_mode_json_buffer[700];
    bool save_to_eeprom = false;

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "c5") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "c6") == 0) {
            charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "c7") == 0) {
            charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "c8") == 0) {
            charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "c9") == 0) {
            charge_mode_config.eeprom_charge_mode_data.decimal_places = (decimal_places_t) atoi(values[idx]);
        }
        
        // Pre charge related settings
        else if (strcmp(params[idx], "c10") == 0) {
            charge_mode_config.eeprom_charge_mode_data.precharge_enable = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "c11") == 0) {
            charge_mode_config.eeprom_charge_mode_data.precharge_time_ms = strtol(values[idx], NULL, 10);
        }
        else if (strcmp(params[idx], "c12") == 0) {
            charge_mode_config.eeprom_charge_mode_data.precharge_speed_rps = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "c13") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_stop_gate_ratio = strtof(values[idx], NULL);
        }

        // AI tuning related settings
        else if (strcmp(params[idx], "c14") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_time_target_ms = (uint32_t) strtol(values[idx], NULL, 10);
        }
        else if (strcmp(params[idx], "c15") == 0) {
            charge_mode_config.eeprom_charge_mode_data.total_time_target_ms = (uint32_t) strtol(values[idx], NULL, 10);
        }
        else if (strcmp(params[idx], "c16") == 0) {
            charge_mode_config.eeprom_charge_mode_data.ml_data_collection_enabled = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "c17") == 0) {
            charge_mode_config.eeprom_charge_mode_data.auto_zero_on_cup_return = string_to_boolean(values[idx]);
        }

        // Pulse mode settings
        else if (strcmp(params[idx], "c18") == 0) {
            charge_mode_config.eeprom_charge_mode_data.pulse_mode_enabled = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "c19") == 0) {
            float val = strtof(values[idx], NULL);
            charge_mode_config.eeprom_charge_mode_data.pulse_threshold = fmaxf(0.3f, fminf(1.0f, val));
        }
        else if (strcmp(params[idx], "c20") == 0) {
            charge_mode_config.eeprom_charge_mode_data.pulse_duration_ms = (uint32_t) strtol(values[idx], NULL, 10);
        }
        else if (strcmp(params[idx], "c21") == 0) {
            charge_mode_config.eeprom_charge_mode_data.pulse_wait_ms = (uint32_t) strtol(values[idx], NULL, 10);
        }

        // Scale stabilization settings
        else if (strcmp(params[idx], "c22") == 0) {
            charge_mode_config.eeprom_charge_mode_data.stabilization_enabled = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "c23") == 0) {
            charge_mode_config.eeprom_charge_mode_data.stabilization_time_ms = (uint32_t) strtol(values[idx], NULL, 10);
        }

        // LED related settings
        else if (strcmp(params[idx], "c1") == 0) {
            charge_mode_config.eeprom_charge_mode_data.neopixel_normal_charge_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "c2") == 0) {
            charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "c3") == 0) {
            charge_mode_config.eeprom_charge_mode_data.neopixel_over_charge_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "c4") == 0) {
            charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }
    
    // Perform action
    if (save_to_eeprom) {
        charge_mode_config_save();
    }

    // Response
    snprintf(charge_mode_json_buffer, 
             sizeof(charge_mode_json_buffer),
             "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
             "{\"c1\":\"#%06lx\",\"c2\":\"#%06lx\",\"c3\":\"#%06lx\",\"c4\":\"#%06lx\","
             "\"c5\":%.3f,\"c6\":%.3f,\"c7\":%.3f,\"c8\":%.3f,\"c9\":%d,\"c10\":%s,\"c11\":%ld,\"c12\":%0.3f,\"c13\":%0.3f,"
             "\"c14\":%lu,\"c15\":%lu,\"c16\":%s,\"c17\":%s,"
             "\"c18\":%s,\"c19\":%.3f,\"c20\":%lu,\"c21\":%lu,"
             "\"c22\":%s,\"c23\":%lu}",
             charge_mode_config.eeprom_charge_mode_data.neopixel_normal_charge_colour._raw_colour,
             charge_mode_config.eeprom_charge_mode_data.neopixel_under_charge_colour._raw_colour,
             charge_mode_config.eeprom_charge_mode_data.neopixel_over_charge_colour._raw_colour,
             charge_mode_config.eeprom_charge_mode_data.neopixel_not_ready_colour._raw_colour,
             charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold,
             charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold,
             charge_mode_config.eeprom_charge_mode_data.set_point_sd_margin,
             charge_mode_config.eeprom_charge_mode_data.set_point_mean_margin,
             charge_mode_config.eeprom_charge_mode_data.decimal_places,
             boolean_to_string(charge_mode_config.eeprom_charge_mode_data.precharge_enable),
             charge_mode_config.eeprom_charge_mode_data.precharge_time_ms,
             charge_mode_config.eeprom_charge_mode_data.precharge_speed_rps,
             charge_mode_config.eeprom_charge_mode_data.coarse_stop_gate_ratio,
             (unsigned long) charge_mode_config.eeprom_charge_mode_data.coarse_time_target_ms,
             (unsigned long) charge_mode_config.eeprom_charge_mode_data.total_time_target_ms,
             boolean_to_string(charge_mode_config.eeprom_charge_mode_data.ml_data_collection_enabled),
             boolean_to_string(charge_mode_config.eeprom_charge_mode_data.auto_zero_on_cup_return),
             boolean_to_string(charge_mode_config.eeprom_charge_mode_data.pulse_mode_enabled),
             charge_mode_config.eeprom_charge_mode_data.pulse_threshold,
             (unsigned long) charge_mode_config.eeprom_charge_mode_data.pulse_duration_ms,
             (unsigned long) charge_mode_config.eeprom_charge_mode_data.pulse_wait_ms,
             boolean_to_string(charge_mode_config.eeprom_charge_mode_data.stabilization_enabled),
             (unsigned long) charge_mode_config.eeprom_charge_mode_data.stabilization_time_ms);

    size_t data_length = strlen(charge_mode_json_buffer);
    file->data = charge_mode_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_charge_mode_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // s0 (float): Charge weight set point (unitless)
    // s1 (float): Current weight (unitless)
    // s2 (charge_mode_state_t | int): Charge mode state
    // s3 (uint32_t): Charge mode event
    // s4 (string): Profile Name
    // s5 (string): Elapsed time in seconds, live during charging

    static char charge_mode_json_buffer[160];  // Increased to fit s5
    char elapsed_time_buffer[16] = {0};

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "s0") == 0) {
            charge_mode_config.target_charge_weight = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "s2") == 0) {
            charge_mode_state_t new_state = (charge_mode_state_t) atoi(values[idx]);

            // Exit
            if (new_state == CHARGE_MODE_EXIT && charge_mode_config.charge_mode_state != CHARGE_MODE_EXIT) {
                ButtonEncoderEvent_t button_event = BUTTON_RST_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, portMAX_DELAY);
            }
            // Enter
            else if (new_state == CHARGE_MODE_WAIT_FOR_ZERO && charge_mode_config.charge_mode_state == CHARGE_MODE_EXIT) {
                // Set exit_status for the menu
                exit_state = APP_STATE_ENTER_CHARGE_MODE_FROM_REST;

                // Then signal the menu to stop
                ButtonEncoderEvent_t button_event = OVERRIDE_FROM_REST;
                xQueueSend(encoder_event_queue, &button_event, portMAX_DELAY);
            }

            charge_mode_config.charge_mode_state = new_state;
        }
    }

    // Handle the special case
    float current_measurement = scale_get_current_measurement();
    char weight_string[16];
    if (isnanf(current_measurement)) {
        sprintf(weight_string, "\"nan\"");
    }
    else if (isinff(current_measurement)) {
        sprintf(weight_string, "\"inf\"");
    }
    else {
        sprintf(weight_string, "%0.3f", current_measurement);
    }

    // Format elapsed time
    if (charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_COMPLETE) {
        TickType_t now = xTaskGetTickCount();
        float elapsed_seconds = (float)((now - charge_start_tick) * portTICK_PERIOD_MS) / 1000.0f;
        snprintf(elapsed_time_buffer, sizeof(elapsed_time_buffer), "%.2f", elapsed_seconds);
    } else {
        snprintf(elapsed_time_buffer, sizeof(elapsed_time_buffer), "%.2f", last_charge_elapsed_seconds);
    }

    // Response
    snprintf(charge_mode_json_buffer, 
             sizeof(charge_mode_json_buffer),
             "%s"
             "{\"s0\":%0.3f,\"s1\":%s,\"s2\":%d,\"s3\":%lu,\"s4\":\"%s\",\"s5\":\"%s\"}",
             http_json_header,
             charge_mode_config.target_charge_weight,
             weight_string,
             (int) charge_mode_config.charge_mode_state,
             charge_mode_config.charge_mode_event,
             profile_get_selected()->name,
             elapsed_time_buffer);

    // Clear events
    charge_mode_config.charge_mode_event = 0;

    size_t data_length = strlen(charge_mode_json_buffer);
    file->data = charge_mode_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
