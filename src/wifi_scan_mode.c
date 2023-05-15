#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#include "wifi_scan_mode.h"
#include "display.h"
#include "u8g2.h"


TaskHandle_t wifi_scan_render_task_handler = NULL;


void wifi_scan_display_render_task(void *p) {
    u8g2_t * display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(display_handler);

        // Draw title
        u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
        u8g2_DrawStr(display_handler, 5, 10, "Wifi Scan");

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


uint8_t wifi_scan() {
    if (wifi_scan_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(wifi_scan_display_render_task, "Wifi Scan Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &wifi_scan_render_task_handler);
    }
    else {
        vTaskResume(wifi_scan_render_task_handler);
    }

    sleep_ms(10000);


    vTaskSuspend(wifi_scan_render_task_handler);


    return 1;  // Move to show page
}