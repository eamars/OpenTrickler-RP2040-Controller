#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "activity.h"
#include "app.h"
#include "events.h"
#include "u8g2.h"
#include "mini_12864_module.h"
#include "motors.h"
#include "scale.h"
#include "display.h"
#include "common.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "servo_gate.h"
#include "menu.h"

// How often a running cleanup looks at the motor drivers when no button is pressed (ADR-012, step 6)
#define CLEANUP_MODE_FAULT_CHECK_MS 100


// Memory from other modules
extern QueueHandle_t encoder_event_queue;
extern charge_mode_config_t charge_mode_config;
extern servo_gate_t servo_gate;
extern AppState_t exit_state;
extern QueueHandle_t encoder_event_queue;

// Internal
cleanup_mode_config_t cleanup_mode_config;


static char title_string[30];
TaskHandle_t cleanup_render_task_handler = NULL;

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


void cleanup_render_task(void *p) {
    char buf[32];
    float prev_weight = 0;

    u8g2_t * display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(display_handler);

        // Draw title
        if (strlen(title_string)) {
            u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(display_handler, 5, 10, title_string);
        }

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        // Draw charge weight
        float current_weight = scale_get_current_measurement();
        memset(buf, 0x0, sizeof(buf));
        
        // Convert to weight string with given decimal places
        // Room for any weight the scale can send, and written with its length: "-1000.00" is a byte more than the
        // 8 this had, and a tared container lifted off the pan is that (ADR-012; audit M5)
        char weight_string[16];
        weight_to_string(weight_string, sizeof(weight_string), current_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);

        snprintf(buf, sizeof(buf), "Weight: %s", weight_string);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 25, buf);

        // Draw flow rate
        float weight_diff = current_weight - prev_weight;
        prev_weight = current_weight;
        float flow_rate = weight_diff / 0.02;  // 20 ms per sampling period, see below

        memset(buf, 0x0, sizeof(buf));
        snprintf(buf, sizeof(buf), "Flow: %0.3f/s", flow_rate);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 35, buf);

        // Draw current motor speed
        memset(buf, 0x0, sizeof(buf));
        snprintf(buf, sizeof(buf), "Speed: %0.3f", cleanup_mode_config.trickler_speed);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 45, buf);

        memset(buf, 0x0, sizeof(buf));
        snprintf(buf, sizeof(buf), "Servo Gate: %s", gate_state_to_string(servo_gate.gate_state));
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 55, buf);

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
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


uint8_t cleanup_mode_menu() {
    // No cleanup with a motor driver not OK, from the menu or the web (ADR-012): the fault screen, before this mode
    // draws anything (step 6). motors_ready() sets up again each driver not OK, so a passing fault does not refuse
    if (!motors_ready()) {
        printf("Cleanup: not started, %s\n", motors_get_fault());
        uint8_t exit_form_id = menu_motor_fault_view();
        cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_EXIT;
        return exit_form_id;
    }

    // If the display task is never created then we shall create one, otherwise we shall resume the task
    if (cleanup_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(cleanup_render_task, "Cleanup Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &cleanup_render_task_handler);
    }
    else {
        vTaskResume(cleanup_render_task_handler);
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
    if (servo_gate.gate_state != GATE_DISABLED) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_OPEN, true);
    }

    // Update current status
    snprintf(title_string, sizeof(title_string), "Adjust Speed");

    bool quit = false;
    while (!quit) {
        // Wait for a button, looking at the motor drivers between presses
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input_ms(CLEANUP_MODE_FAULT_CHECK_MS);

        // A motor driver not OK stops both motors and ends the cleanup, naming it (ADR-012, step 6); stopped at the
        // fault, by the unit, and in `fault` until a press or a stop acknowledges it (gate 4)
        const char * motor_fault = motors_get_fault();
        if (motor_fault != NULL) {
            cleanup_mode_config.trickler_speed = 0;
            motor_set_speed(SELECT_BOTH_MOTOR, 0);
            snprintf(title_string, sizeof(title_string), "%s", motor_fault);
            printf("Cleanup: ended, %s\n", motor_fault);
            cleanup_in_fault = true;
            cleanup_mode_log(EVENT_STOPPED, ACTIVITY_BY_UNIT, EVENT_REASON_FAULT);
            stop_logged = true;

            while (button_wait_for_input_ms(50) == BUTTON_NO_EVENT && !activity_take_stop(NULL)) {
            }
            break;
        }

        // A stop asked by the API or the console, as the reset button
        activity_by_t stop_by;
        if (activity_take_stop(&stop_by)) {
            button_encoder_event = BUTTON_RST_PRESSED;
            end_by = stop_by;
        }
        // A speed given with a time runs out: the cleanup stops by itself
        else if (cleanup_has_deadline && (int32_t) (xTaskGetTickCount() - cleanup_deadline_tick) >= 0) {
            button_encoder_event = BUTTON_RST_PRESSED;
            end_by = ACTIVITY_BY_UNIT;
            end_reason = EVENT_REASON_TIME;
        }

        switch (button_encoder_event) {
            case BUTTON_RST_PRESSED:
                cleanup_mode_config.trickler_speed = 0;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                quit = true;

                break;
            case BUTTON_ENCODER_ROTATE_CW:
                // A person at the unit: until stopped, as there
                cleanup_has_deadline = false;
                cleanup_mode_config.trickler_speed = cleanup_mode_limit_speed(cleanup_mode_config.trickler_speed + 1,
                                                                              cleanup_mode_config.trickler_speed);
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                break;
            case BUTTON_ENCODER_ROTATE_CCW:
                cleanup_has_deadline = false;
                cleanup_mode_config.trickler_speed = cleanup_mode_limit_speed(cleanup_mode_config.trickler_speed - 1,
                                                                              cleanup_mode_config.trickler_speed);
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                break;

            case BUTTON_ENCODER_PRESSED:
                cleanup_has_deadline = false;
                cleanup_mode_config.trickler_speed = 0;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);

                break;
            default:
                break;
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

    vTaskSuspend(cleanup_render_task_handler);
    return 1;  // Return backs to the main menu view
}


cleanup_start_result_t cleanup_mode_request_start(activity_by_t by) {
    // A job, a start asked already, or a person at the unit in a mode or a view of the menu's: not taken over. In
    // the charge's order: busy, then a driver's fault
    activity_t asked_before;
    if (activity_get() != ACTIVITY_NONE || activity_start_pending(&asked_before) || menu_get_current_form_id() < 0) {
        return CLEANUP_START_BUSY;
    }
    if (motors_get_fault() != NULL) {
        return CLEANUP_START_DRIVER_FAULT;
    }
    if (!activity_request_start(ACTIVITY_CLEANUP, by)) {
        return CLEANUP_START_BUSY;
    }
    menu_wake();
    return CLEANUP_START_OK;
}


bool cleanup_mode_set_speed(float speed_rps, float time_s) {
    activity_lock();
    bool running = cleanup_mode_running;
    if (running) {
        cleanup_mode_config.trickler_speed = cleanup_mode_limit_speed(speed_rps, cleanup_mode_config.trickler_speed);
        cleanup_has_deadline = isfinite(time_s);
        if (cleanup_has_deadline) {
            cleanup_deadline_tick = xTaskGetTickCount() + pdMS_TO_TICKS((uint32_t) (time_s * 1000.0f));
        }
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
    status->speed_rps = running ? cleanup_mode_config.trickler_speed : 0.0f;
    status->time_left_s = NAN;
    if (running && cleanup_has_deadline) {
        int32_t left_ticks = (int32_t) (cleanup_deadline_tick - xTaskGetTickCount());
        status->time_left_s = left_ticks > 0 ? (float) (left_ticks * portTICK_PERIOD_MS) / 1000.0f : 0.0f;
    }
}
