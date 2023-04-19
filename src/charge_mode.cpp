#include "app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include "rotary_button.h"
#include "u8g2.h"
#include "FloatRingBuffer.h"

#include "scale.h"

extern u8g2_t display_handler;
extern uint8_t charge_weight_digits[];
extern QueueHandle_t encoder_event_queue;

// Configures
float target_charge_weight = 0.0f;
float cfg_zero_sd_threshold = 0.02;
float cfg_zero_mean_threshold = 0.04;
TaskHandle_t scale_measurement_render_task_handler = NULL;
static char title_string[30];


typedef enum {
    CHARGE_MODE_WAIT_FOR_ZERO,
    CHARGE_MODE_WAIT_FOR_COMPLETE,
    CHARGE_MODE_WAIT_FOR_PAN_REMOVAL,
    CHARGE_MODE_EXIT,

} ChargeModeState_t;


void scale_measurement_render_task(void *p) {
    char current_weight_string[5];

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(&display_handler);
        // Draw title
        if (strlen(title_string)) {
            u8g2_SetFont(&display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(&display_handler, 5, 10, title_string);
        }

        // Draw line
        u8g2_DrawHLine(&display_handler, 0, 13, u8g2_GetDisplayWidth(&display_handler));

        // current weight
        memset(current_weight_string, 0x0, sizeof(current_weight_string));
        sprintf(current_weight_string, "%0.02f", scale_get_current_measurement());

        u8g2_SetFont(&display_handler, u8g2_font_profont22_tf);
        u8g2_DrawStr(&display_handler, 26, 35, current_weight_string);

        // print unit
        u8g2_SetFont(&display_handler, u8g2_font_helvR08_tr);
        u8g2_DrawStr(&display_handler, 96, 35, "gr");

        u8g2_SendBuffer(&display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


ChargeModeState_t charge_mode_wait_for_zero(ChargeModeState_t prev_state) {
    // Wait for 5 measurements and wait for stable
    FloatRingBuffer data_buffer(10);

    // Update current status
    snprintf(title_string, sizeof(title_string), "Zeroing..");

    while (true) {
        float current_measurement = scale_block_wait_for_next_measurement();
        TickType_t last_measurement_tick = xTaskGetTickCount();

        data_buffer.enqueue(current_measurement);

        if (data_buffer.getCounter() >= 10){
            float sd = data_buffer.getSd();
            float mean = abs(data_buffer.getMean());
            if (sd < cfg_zero_sd_threshold && mean < cfg_zero_mean_threshold){
                printf("Zero and stable condition reached\n");
                break;
            }
        }

        // Wait if quit is pressed
        ButtonEncoderEvent_t button_encoder_event;
        while (xQueueReceive(encoder_event_queue, &button_encoder_event, 0)){
            if (button_encoder_event == BUTTON_RST_PRESSED) {
                return CHARGE_MODE_EXIT;
            }
        }

        // Wait for 0.5s for next measurement
        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(100));
    }

    return CHARGE_MODE_WAIT_FOR_COMPLETE;
}

ChargeModeState_t charge_mode_wait_for_complete(ChargeModeState_t prev_state) {
    // Update current status
    snprintf(title_string, sizeof(title_string), "Target: %.02f gr", target_charge_weight);

    while (true) {
        // Wait if quit is pressed
        ButtonEncoderEvent_t button_encoder_event;
        while (xQueueReceive(encoder_event_queue, &button_encoder_event, 0)){
            if (button_encoder_event == BUTTON_RST_PRESSED) {
                return CHARGE_MODE_EXIT;
            }
        }
    }


    return CHARGE_MODE_WAIT_FOR_PAN_REMOVAL;
}

ChargeModeState_t charge_mode_wait_for_pan_removal(ChargeModeState_t prev_state) {
    return CHARGE_MODE_WAIT_FOR_ZERO;
}


uint8_t charge_mode_menu() {
    // Create target weight
    target_charge_weight = charge_weight_digits[3] * 10 + \
                           charge_weight_digits[2] + \
                           charge_weight_digits[1] * 0.1 + \
                           charge_weight_digits[0] * 0.01;
    printf("Target Charge Weight: %f\n", target_charge_weight);

    // If the display task is never created then we shall create one, otherwise we shall resume the task
    if (scale_measurement_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(scale_measurement_render_task, "Scale Measurement Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &scale_measurement_render_task_handler);
    }
    else {
        vTaskResume(scale_measurement_render_task_handler);
    }
    
    ChargeModeState_t state = CHARGE_MODE_WAIT_FOR_ZERO;

    bool quit = false;
    while (quit == false) {
        switch (state) {
            case CHARGE_MODE_WAIT_FOR_ZERO:
                state = charge_mode_wait_for_zero(state);
                break;
            case CHARGE_MODE_WAIT_FOR_COMPLETE:
                state = charge_mode_wait_for_complete(state);
                break;
            case CHARGE_MODE_WAIT_FOR_PAN_REMOVAL:
                state = charge_mode_wait_for_pan_removal(state);
                break;
            case CHARGE_MODE_EXIT:
            default:
                quit = true;
                break;
        }

    }
    
    // // Wait for user input
    // ButtonEncoderEvent_t button_encoder_event;
    // while (true) {
    //     while (xQueueReceive(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(20))){
    //         printf("%d\n", button_encoder_event);
    //     }
    // }
    
    // vTaskDelete(scale_measurement_render_handler);
    vTaskSuspend(scale_measurement_render_task_handler);
    return 1;  // return back to main menu
}