#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#include <pico/cyw43_arch.h>

#include "wireless_client_mode.h"
#include "display.h"
#include "u8g2.h"
#include "cyw43_control.h"

#define MAX_NUM_SSID        16
#define MAX_SSID_LENGTH     32

TaskHandle_t wifi_scan_render_task_handler = NULL;

int num_ssid_scanned = 0;
int16_t wifi_selected_ssid = 0;
char ssid_list[MAX_NUM_SSID][MAX_SSID_LENGTH];  // 32 chars per SSID and 16 SSIDs in total
char status[16];


void wifi_scan_display_render_task(void *p) {
    u8g2_t * display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();
        char buf[30];

        u8g2_ClearBuffer(display_handler);

        // Draw title
        u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
        u8g2_DrawStr(display_handler, 5, 10, "Wifi Scan");

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        // Draw status
        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Num SSID: %d", num_ssid_scanned);
        snprintf(buf, sizeof(buf), status);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 25, buf);

        // Draw number of SSIDs detected
        memset(buf, 0x0, sizeof(buf));
        sprintf(buf, "Num SSID: %d", num_ssid_scanned);
        u8g2_SetFont(display_handler, u8g2_font_profont11_tf);
        u8g2_DrawStr(display_handler, 5, 35, buf);

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        // printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
        //     result->ssid, result->rssi, result->channel,
        //     result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
        //     result->auth_mode);
        if (strlen(result->ssid) > 0 && num_ssid_scanned < MAX_NUM_SSID) {
            memcpy(ssid_list[num_ssid_scanned], 
                result->ssid, 
                MAX_SSID_LENGTH);
            num_ssid_scanned += 1;
        }

    }
    return 0;
}


uint8_t wifi_scan() {
    num_ssid_scanned = 0;
    memset(status, 0x0, sizeof(status));

    if (wifi_scan_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(wifi_scan_display_render_task, "Wifi Scan Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &wifi_scan_render_task_handler);
    }
    else {
        vTaskResume(wifi_scan_render_task_handler);
    }

    cyw43_arch_enable_sta_mode();

    snprintf(status, sizeof(status), "> Scanning");

    // Perform scan once
    cyw43_wifi_scan_options_t scan_options = {0};
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
    if (err) {
        exit(err);
    }

    while (cyw43_wifi_scan_active(&cyw43_state)) {
        vTaskDelay(500);
    }

    // Show complete for 3 seconds
    snprintf(status, sizeof(status), "> Complete");
    vTaskDelay(500);
    

    vTaskSuspend(wifi_scan_render_task_handler);


    return 72;  // Move to FORM 72
}


const char * wifi_get_ssid_name(void *data, uint16_t index) {
    return ssid_list[index];
}

uint16_t wifi_get_ssid_count(void *data) {
    return num_ssid_scanned;
}