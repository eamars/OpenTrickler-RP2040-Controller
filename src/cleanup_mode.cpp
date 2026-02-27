#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "app.h"
#include "u8g2.h"
#include "mini_12864_module.h"
#include "motors.h"
#include "scale.h"
#include "display.h"
#include "common.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "servo_gate.h"


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
        char weight_string[WEIGHT_STRING_LEN];
        float_to_string(weight_string, current_weight, charge_mode_config.eeprom_charge_mode_data.decimal_places);

        sprintf(buf, "Weight: %s", weight_string);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 25, buf);

        // Draw flow rate
        float weight_diff = current_weight - prev_weight;
        prev_weight = current_weight;
        float flow_rate = weight_diff / 0.02;  // 20 ms per sampling period, see below

        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Flow: %0.3f/s", flow_rate);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 35, buf);

        // Draw current motor speed
        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Speed: %0.3f", cleanup_mode_config.trickler_speed);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 45, buf);

        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Servo Gate: %s", gate_state_to_string(servo_gate.gate_state));
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 55, buf);

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


uint8_t cleanup_mode_menu() {
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

    // Enter the clean up mode
    cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_ENTER;

    // Enable both motors
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);

    // Open servo gate (if enabled)
    if (servo_gate.gate_state != GATE_DISABLED) {
        servo_gate_set_ratio(SERVO_GATE_RATIO_OPEN, true);
    }

    // Update current status
    snprintf(title_string, sizeof(title_string), "Adjust Speed");

    bool quit = false;
    while (!quit) {
        // Wait if button is pressed
        ButtonEncoderEvent_t button_encoder_event;
        xQueueReceive(encoder_event_queue, &button_encoder_event, portMAX_DELAY);

        switch (button_encoder_event) {
            case BUTTON_RST_PRESSED:
                cleanup_mode_config.trickler_speed = 0;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                quit = true;

                break;
            case BUTTON_ENCODER_ROTATE_CW:
                cleanup_mode_config.trickler_speed += 1;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                break;
            case BUTTON_ENCODER_ROTATE_CCW:
                cleanup_mode_config.trickler_speed -= 1;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                break;

            case BUTTON_ENCODER_PRESSED:
                cleanup_mode_config.trickler_speed = 0;
                motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
                
                break;
            default:
                break;
        }
        
    }

    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);

    cleanup_mode_config.cleanup_mode_state = CLEANUP_MODE_EXIT;

    vTaskSuspend(cleanup_render_task_handler);
    return 1;  // Return backs to the main menu view
}


bool http_rest_cleanup_mode_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // s0 (cleanup_mode_state_t | int): Cleanup mode state
    // s1 (float): Trickler speed

    static char cleanup_mode_json_buffer[128];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "s0") == 0) {
            cleanup_mode_state_t new_state = (cleanup_mode_state_t) atoi(values[idx]);

            // Exit
            if (new_state == CLEANUP_MODE_EXIT && cleanup_mode_config.cleanup_mode_state != CLEANUP_MODE_EXIT) {
                ButtonEncoderEvent_t button_event = BUTTON_RST_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, portMAX_DELAY);
            }

            // Enter
            else if (new_state == CLEANUP_MODE_ENTER && cleanup_mode_config.cleanup_mode_state != CLEANUP_MODE_ENTER) {
                // Set exit_status for the menu
                exit_state = APP_STATE_ENTER_CLEANUP_MODE;

                // Then signal the menu to stop
                ButtonEncoderEvent_t button_event = OVERRIDE_FROM_REST;
                xQueueSend(encoder_event_queue, &button_event, portMAX_DELAY);
            }

            cleanup_mode_config.cleanup_mode_state = new_state;
        }
        else if (strcmp(params[idx], "s1") == 0) {
            cleanup_mode_config.trickler_speed = strtof(values[idx], NULL);
            motor_set_speed(SELECT_BOTH_MOTOR, cleanup_mode_config.trickler_speed);
        }
    }

    // Response
    snprintf(cleanup_mode_json_buffer, 
             sizeof(cleanup_mode_json_buffer),
             "%s"
             "{\"s0\":%d,\"s1\":%0.3f}",
             http_json_header,
             (int) cleanup_mode_config.cleanup_mode_state,
             cleanup_mode_config.trickler_speed);


    size_t data_length = strlen(cleanup_mode_json_buffer);
    file->data = cleanup_mode_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;


    return true;
}
