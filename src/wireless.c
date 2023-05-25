#include <pico/cyw43_arch.h>
#include <FreeRTOS.h>
#include <queue.h>

// #include <lwip/apps/httpd.h>

#include "wireless.h"
#include "eeprom.h"
#include "access_point_mode.h"
#include "display.h"
#include "rotary_button.h"
#include "http_rest.h"


typedef enum {
    WIRELESS_STATE_NOT_INITIALIZED = 0,
    WIRELESS_STATE_IDLE,
    WIRELESS_STATE_AP_MODE_INIT,
    WIRELESS_STATE_AP_MODE_LISTEN,
    WIRELESS_STATE_STA_MODE_INIT,
    WIRELESS_STATE_STA_MODE_LISTEN,
} wireless_state_t;

typedef enum {
    WIRELESS_CTRL_CYW43_INIT = 0,
    WIRELESS_CTRL_CYW43_DEINIT,
    WIRELESS_CTRL_DISCONNECT,
    WRIELESS_CTRL_START_AP_MODE,
    WRIELESS_CTRL_START_STA_MODE,
} wireless_ctrl_t;


typedef struct {
    eeprom_wireless_metadata_t eeprom_wireless_metadata;
    wireless_state_t current_wireless_state;
} wireless_config_t;


static TaskHandle_t wirelss_info_render_task_handler = NULL;
static wireless_config_t wireless_config;
static QueueHandle_t wireless_ctrl_queue;

// Render task
const char * wireless_state_strings[] = {
    "Not Initialized",
    "Idling",
    "AP Mode Init",
    "AP Mode Listen",
    "STA Mode Init",
    "STA Mode Listen"
};

char ip_addr_string[16];
char first_line_buffer[32];
char second_line_buffer[32];


bool wireless_config_init() {
    bool is_ok = true;

    memset(&wireless_config, 0x00, sizeof(wireless_config_t));

    is_ok = eeprom_read(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_WIRELESS_CONFIG_BASE_ADDR);
        return false;
    }

    // If the revision doesn't match then re-initialize the config
    if (wireless_config.eeprom_wireless_metadata.wireless_data_rev != EEPROM_WIRELESS_CONFIG_METADATA_REV) {
        wireless_config.eeprom_wireless_metadata.wireless_data_rev = EEPROM_WIRELESS_CONFIG_METADATA_REV;
        wireless_config.eeprom_wireless_metadata.configured = false;

        // Write data back
        is_ok = eeprom_write(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_WIRELESS_CONFIG_BASE_ADDR);
            return false;
        }
    }

    is_ok = eeprom_write(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));

    return is_ok;
}


bool http_rest_eeprom_handler(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "It Works";
    file->len = 9;
    file->index = 9;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


void wireless_task(void *p) {
    wireless_config.current_wireless_state = WIRELESS_STATE_NOT_INITIALIZED;
    wireless_ctrl_queue = xQueueCreate(5, sizeof(wireless_ctrl_t));

    if (cyw43_arch_init()) {
        exit(-1);
    }

    wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;

    // Start default initialize pattern
    if (wireless_config.eeprom_wireless_metadata.configured) {
        wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_INIT;
        cyw43_arch_enable_sta_mode();

        int resp;
        resp = cyw43_arch_wifi_connect_blocking(wireless_config.eeprom_wireless_metadata.ssid, 
                                                wireless_config.eeprom_wireless_metadata.pw,
                                                wireless_config.eeprom_wireless_metadata.auth);
        if (resp) {
            // Failed to connect, fallback to AP mode
            wireless_config.eeprom_wireless_metadata.configured = false;
        }

        wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
    }


    // If not configured, or failed to connect existing wifi then start the AP mode
    if (!wireless_config.eeprom_wireless_metadata.configured) {
        // If previous configured, then start STA mode by default
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_INIT;
        access_point_mode_start();
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_LISTEN;
    }

    // Start the HTTP server
    httpd_init();

    rest_register_handler("/rest/eeprom", http_rest_eeprom_handler);


    while (true) {
        wireless_ctrl_t wireless_ctrl;

        xQueueReceive(wireless_ctrl_queue, &wireless_ctrl, portMAX_DELAY);

        // TODO: Implement control
        switch (wireless_ctrl) {
            default:
                break;
        }
    }

    cyw43_arch_deinit();
}



void wirelss_info_render_task(void *p) {
    u8g2_t * display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(display_handler);

        // Draw state in the title
        const char * title_string = wireless_state_strings[wireless_config.current_wireless_state];
        u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
        u8g2_DrawStr(display_handler, 5, 10, title_string);

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        // Draw IP address
        if (strlen(ip_addr_string)) {
            u8g2_SetFont(display_handler, u8g2_font_6x12_tf);
            u8g2_DrawStr(display_handler, 5, 23, ip_addr_string);
        }

        // Draw first line
        if (strlen(first_line_buffer)) {
            u8g2_SetFont(display_handler, u8g2_font_6x12_tf);
            u8g2_DrawStr(display_handler, 5, 33, first_line_buffer);
        }

        // Draw second line
        if (strlen(second_line_buffer)) {
            u8g2_SetFont(display_handler, u8g2_font_6x12_tf);
            u8g2_DrawStr(display_handler, 5, 43, second_line_buffer);
        }

        // Draw OK button
        u8g2_SetFont(display_handler, u8g2_font_helvR08_tr);
        u8g2_DrawButtonUTF8(display_handler, 64, 59, U8G2_BTN_HCENTER | U8G2_BTN_INV | U8G2_BTN_BW1, 0, 1, 1, " OK ");

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(200));
    }
}


uint8_t wireless_view_wifi_info(void) {
    if (wirelss_info_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(wirelss_info_render_task, "AP Mode Display Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &wirelss_info_render_task_handler);
    }
    else {
        vTaskResume(wirelss_info_render_task_handler);
    }

    bool quit = false;
    while (quit == false) {
        // Wait if quit is pressed
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(true);
        if (button_encoder_event == BUTTON_RST_PRESSED || button_encoder_event == BUTTON_ENCODER_PRESSED) {
            quit = true;
            break;
        }
    }

    vTaskSuspend(wirelss_info_render_task_handler);

    return 40;  // Returns to the Wireless menu (view 40)
}