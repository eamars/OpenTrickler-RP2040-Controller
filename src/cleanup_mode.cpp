#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "app.h"
#include "u8g2.h"
#include "rotary_button.h"
#include "motors.h"
#include "scale.h"
#include "display.h"
#include "common.h"
#include "charge_mode.h"


// Memory from other modules
extern QueueHandle_t encoder_event_queue;
extern charge_mode_config_t charge_mode_config;


static char title_string[30];
TaskHandle_t cleanup_render_task_handler = NULL;
float current_motor_speed = 0;

int motor_select_index = 0;
const motor_select_t available_motor_select[] = {
    SELECT_BOTH_MOTOR,
    SELECT_COARSE_TRICKLER_MOTOR,
    SELECT_FINE_TRICKLER_MOTOR,
};


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
        sprintf(buf, "Speed: %d", (int) current_motor_speed);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 45, buf);

        // Draw current selected motor
        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Select: %s motor", get_motor_select_string(available_motor_select[motor_select_index]));
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

    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);

    current_motor_speed = 0;

    // Update current status
    snprintf(title_string, sizeof(title_string), "Adjust Speed");

    bool quit = false;
    while (!quit) {
        // Wait if button is pressed
        ButtonEncoderEvent_t button_encoder_event;
        xQueueReceive(encoder_event_queue, &button_encoder_event, portMAX_DELAY);

        switch (button_encoder_event) {
            case BUTTON_RST_PRESSED:
                quit = true;
                break;
            case BUTTON_ENCODER_ROTATE_CW:
                current_motor_speed += 1;
                motor_set_speed(available_motor_select[motor_select_index], current_motor_speed);
                break;
            case BUTTON_ENCODER_ROTATE_CCW:
                current_motor_speed -= 1;
                motor_set_speed(available_motor_select[motor_select_index], current_motor_speed);
                break;

            case BUTTON_ENCODER_PRESSED:
                // If current speed is non zero then set speed to 0 and move to next option
                if (current_motor_speed != 0) {
                    current_motor_speed = 0;
                    motor_set_speed(available_motor_select[motor_select_index], current_motor_speed);
                }
                motor_select_index += 1;
                motor_select_index %= 3;
                motor_set_speed(available_motor_select[motor_select_index], current_motor_speed);
                
                break;
            default:
                break;
        }
        
    }

    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);

    vTaskSuspend(cleanup_render_task_handler);
    return 1;  // Return backs to the main menu view
}