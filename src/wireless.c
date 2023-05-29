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
#include "rest_endpoints.h"

#ifdef CYW43_HOST_NAME
#undef CYW43_HOST_NAME
#endif

#define CYW43_HOST_NAME "OpenTrickler"


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


char first_line_buffer[32];
char second_line_buffer[32];


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
        char * ip_addr_string = ipaddr_ntoa(netif_ip4_addr(netif_default));
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

        // Draw link status
        if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_INIT || 
            wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
                int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
                char * link_status_string = NULL;

                if (link_status == CYW43_LINK_DOWN) {
                    link_status_string = "LINK_DOWN";
                }
                else if (link_status == CYW43_LINK_JOIN) {
                    link_status_string = "CYW43_LINK_JOIN";
                }
                else if (link_status == CYW43_LINK_NOIP) {
                    link_status_string = "CYW43_LINK_NOIP";
                }
                else if (link_status == CYW43_LINK_UP) {
                    link_status_string = "CYW43_LINK_UP";
                }
                else if (link_status == CYW43_LINK_FAIL) {
                    link_status_string = "CYW43_LINK_FAIL";
                }
                else if (link_status == CYW43_LINK_NONET) {
                    link_status_string = "CYW43_LINK_NONET";
                }
                else if (link_status == CYW43_LINK_BADAUTH) {
                    link_status_string = "CYW43_LINK_BADAUTH";
                }
                u8g2_SetFont(display_handler, u8g2_font_6x12_tf);
                u8g2_DrawStr(display_handler, 5, 53, link_status_string);
            }


        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(200));
    }
}



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
        wireless_config.eeprom_wireless_metadata.timeout_ms = 30000;  // 30s

        // Write data back
        is_ok = eeprom_write(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_WIRELESS_CONFIG_BASE_ADDR);
            return false;
        }
    }

    return is_ok;
}


bool wireless_config_save() {
    bool is_ok;

    is_ok = eeprom_write(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
    if (!is_ok) {
        printf("Unable to write to %x\n", EEPROM_WIRELESS_CONFIG_BASE_ADDR);
        return false;
    }

    return true;
}


void wireless_task(void *p) {
    memset(first_line_buffer, 0x0, sizeof(first_line_buffer));
    memset(second_line_buffer, 0x0, sizeof(second_line_buffer));

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

        // Show the current joining SSID
        sprintf(first_line_buffer, ">%s", wireless_config.eeprom_wireless_metadata.ssid);

        // Retry within timeframe
        TickType_t stop_tick = xTaskGetTickCount() + pdMS_TO_TICKS(wireless_config.eeprom_wireless_metadata.timeout_ms);
        while (xTaskGetTickCount() < stop_tick) {
            int resp;
            resp = cyw43_arch_wifi_connect_timeout_ms(wireless_config.eeprom_wireless_metadata.ssid,
                                                    wireless_config.eeprom_wireless_metadata.pw,
                                                    wireless_config.eeprom_wireless_metadata.auth,
                                                    wireless_config.eeprom_wireless_metadata.timeout_ms);
            if (resp == PICO_OK) {
                wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
                break;
            }
            // else if (resp == PICO_ERROR_BADAUTH) {
            //     // Failed to connect, fallback to AP mode
            //     wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;
            //     break;
            // }
            else {
                // PICO_ERROR_CONNECT_FAILED
                // retry
            }
        }
    }

    // If the state didn't change (connection failed) then we shall put it back to idle
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_INIT) {
         wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;
    }


    // If not configured, or failed to connect existing wifi then start the AP mode
    if (wireless_config.current_wireless_state == WIRELESS_STATE_IDLE) {
        // If previous configured, then start STA mode by default
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_INIT;
        access_point_mode_start();
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_LISTEN;
    }

    // Initialize REST endpoints
    rest_endpoints_init();

    // Start the HTTP server
    httpd_init();

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


bool http_rest_wireless_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char wireless_config_json_buffer[256];

    // If the argument includes control, then update the settings
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "ssid") == 0) {
            memset(wireless_config.eeprom_wireless_metadata.ssid, 0x0, sizeof(wireless_config.eeprom_wireless_metadata.ssid));
            strcpy(wireless_config.eeprom_wireless_metadata.ssid, values[idx]); 
        }

        if (strcmp(params[idx], "pw") == 0) {
            memset(wireless_config.eeprom_wireless_metadata.pw, 0x0, sizeof(wireless_config.eeprom_wireless_metadata.pw));
            strcpy(wireless_config.eeprom_wireless_metadata.pw, values[idx]); 
        }

        if (strcmp(params[idx], "auth") == 0) {
            if (strcmp(values[idx], "CYW43_AUTH_OPEN") == 0) {
                wireless_config.eeprom_wireless_metadata.auth = CYW43_AUTH_OPEN;
            }
            else if (strcmp(values[idx], "CYW43_AUTH_WPA_TKIP_PSK") == 0) {
                wireless_config.eeprom_wireless_metadata.auth = CYW43_AUTH_WPA_TKIP_PSK;
            }
            else if (strcmp(values[idx], "CYW43_AUTH_WPA2_AES_PSK") == 0) {
                wireless_config.eeprom_wireless_metadata.auth = CYW43_AUTH_WPA2_AES_PSK;
            }
            else if (strcmp(values[idx], "CYW43_AUTH_WPA2_MIXED_PSK") == 0) {
                wireless_config.eeprom_wireless_metadata.auth = CYW43_AUTH_WPA2_MIXED_PSK;
            }
        }

        if (strcmp(params[idx], "timeout_ms") == 0) {
            int timeout_ms = strtod(values[idx], NULL);
            wireless_config.eeprom_wireless_metadata.timeout_ms = timeout_ms;
        }

        if (strcmp(params[idx], "configured") == 0 && strcmp(values[idx], "true") == 0) {
            wireless_config.eeprom_wireless_metadata.configured = true;
        }
    }

    // Response
    const char * auth_string;
    switch (wireless_config.eeprom_wireless_metadata.auth) {
        case CYW43_AUTH_OPEN:
            auth_string = "CYW43_AUTH_OPEN";
            break;
        case CYW43_AUTH_WPA_TKIP_PSK:
            auth_string = "CYW43_AUTH_WPA_TKIP_PSK";
            break;
        case CYW43_AUTH_WPA2_AES_PSK:
            auth_string = "CYW43_AUTH_WPA2_AES_PSK";
            break;
        case CYW43_AUTH_WPA2_MIXED_PSK:
            auth_string = "CYW43_AUTH_WPA2_MIXED_PSK";
            break;
        default:
            auth_string = "error";
            break;
    }

    const char * configured_string;
    if (wireless_config.eeprom_wireless_metadata.configured) {
        configured_string = "true";
    }
    else {
        configured_string = "false";
    }

    sprintf(wireless_config_json_buffer, 
            "{\"ssid\":\"%s\",\"pw\":\"%s\",\"auth\":\"%s\",\"timeout_ms\":%d,\"configured\":%s}",
            wireless_config.eeprom_wireless_metadata.ssid,
            wireless_config.eeprom_wireless_metadata.pw,
            auth_string,
            wireless_config.eeprom_wireless_metadata.timeout_ms,
            configured_string);

    size_t data_length = strlen(wireless_config_json_buffer);
    file->data = wireless_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}