#include <FreeRTOS.h>
#include <task.h>
#include <u8g2.h>
#include <string.h>

#include "scale.h"
#include "app.h"
#include "display.h"


char title[16] = "";
char line1[16] = "";
char line2[16] = "";

TaskHandle_t scale_calibration_render_task_handler = NULL;


void scale_calibration_render_task(void *p) {
    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();
        u8g2_t * display_handler = get_display_handler();

        u8g2_ClearBuffer(display_handler);

        // Draw line 1
        if (strlen(line1)) {
            u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(display_handler, 5, 10, line1);
        }

        // Draw line 2
        if (strlen(line2)) {
            u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(display_handler, 5, 25, line2);
        }

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


AppState_t scale_calibrate_with_external_weight(AppState_t prev_state) {
    // TODO: Finish this
    return 0;
}



