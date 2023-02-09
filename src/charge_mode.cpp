#include "app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include "rotary_button.h"
#include "u8g2.h"



extern u8g2_t display_handler;
extern uint8_t charge_weight_digits[];
extern QueueHandle_t encoder_event_queue;
extern float current_scale_measurement;


void scale_measurement_render_task(void *p) {

    while (true) {
        u8g2_ClearBuffer(&display_handler);
        // Draw title
        u8g2_SetFont(&display_handler, u8g2_font_helvB08_tr);
        u8g2_DrawStr(&display_handler, 5, 10, "Zeroing...");

        // Draw line
        u8g2_DrawHLine(&display_handler, 0, 13, u8g2_GetDisplayWidth(&display_handler));

        // current weight
        char current_weight_string[5];
        memset(current_weight_string, 0x0, sizeof(current_weight_string));

        sprintf(current_weight_string, "%0.2f", current_scale_measurement);

        u8g2_SetFont(&display_handler, u8g2_font_profont22_tf);
        u8g2_DrawStr(&display_handler, 26, 35, current_weight_string);

        u8g2_SendBuffer(&display_handler);
    

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


AppState_t charge_mode_menu(AppState_t prev_state) {
    TaskHandle_t scale_measurement_render_handler;
    xTaskCreate(scale_measurement_render_task, "Scale Measurement Render Task", 128, NULL, 1, &scale_measurement_render_handler);

    // Wait for user input
    ButtonEncoderEvent_t button_encoder_event;
    while (true) {
        while (xQueueReceive(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(20))){
            printf("%d\n", button_encoder_event);
        }
    }
    
    vTaskDelete(scale_measurement_render_handler);
    return APP_STATE_DEFAULT;
}