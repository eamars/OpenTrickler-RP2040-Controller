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


extern QueueHandle_t encoder_event_queue;

static char title_string[30];
TaskHandle_t cleanup_render_task_handler = NULL;
float current_motor_speed = 0;

extern motor_config_t coarse_trickler_motor_config;
extern motor_config_t fine_trickler_motor_config;


void cleanup_render_task(void *p) {
    char charge_weight_string[30];
    char current_motor_speed_string[30];

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
        memset(charge_weight_string, 0x0, sizeof(charge_weight_string));
        sprintf(charge_weight_string, "W: %0.02f", scale_get_current_measurement());
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 25, charge_weight_string);

        // Draw current motor speed
        memset(current_motor_speed_string, 0x0, sizeof(current_motor_speed_string));
        sprintf(current_motor_speed_string, "CS: %d", (int) current_motor_speed);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 35, current_motor_speed_string);

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
                motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, current_motor_speed);
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, current_motor_speed);
                
                break;
            case BUTTON_ENCODER_ROTATE_CCW:
                current_motor_speed -= 1;

                motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, current_motor_speed);
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, current_motor_speed);
                break;

            case BUTTON_ENCODER_PRESSED:
                current_motor_speed = 0;
                motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, current_motor_speed);
                motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, current_motor_speed);
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