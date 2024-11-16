#include <pico/cyw43_arch.h>
#include <FreeRTOS.h>
#include <queue.h>

// #include <lwip/apps/httpd.h>

#include "wireless.h"
#include "eeprom.h"
#include "access_point_mode.h"
#include "display.h"
#include "mini_12864_module.h"
#include "http_rest.h"
#include "rest_endpoints.h"
#include "common.h"


#ifdef CYW43_HOST_NAME
#undef CYW43_HOST_NAME
#endif

// Overwrite the host name
#define CYW43_HOST_NAME "OpenTrickler"
#define LED_INTERFACE_MINIMUM_POLL_PERIOD_MS    20

typedef enum {
    WIRELESS_STATE_NOT_INITIALIZED = 0,
    WIRELESS_STATE_IDLE,
    WIRELESS_STATE_AP_MODE_INIT,
    WIRELESS_STATE_AP_MODE_LISTEN,
    WIRELESS_STATE_STA_MODE_INIT,
    WIRELESS_STATE_STA_MODE_LISTEN,
} wireless_state_t;

typedef enum {
    WIRELESS_CTRL_NOP = 0,
    WIRELESS_CTRL_CYW43_INIT,
    WIRELESS_CTRL_CYW43_DEINIT,
    WIRELESS_CTRL_DISCONNECT,
    WRIELESS_CTRL_START_AP_MODE,
    WRIELESS_CTRL_START_STA_MODE,
    WIRELESS_CTRL_LED_ON,
    WIRELESS_CTRL_LED_OFF,
} wireless_ctrl_t;


typedef struct {
    eeprom_wireless_metadata_t eeprom_wireless_metadata;
    wireless_state_t current_wireless_state;
} wireless_config_t;


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


char first_line_buffer[35];
char second_line_buffer[35];


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



bool wireless_init() {
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
        wireless_config.eeprom_wireless_metadata.enable = false;
        wireless_config.eeprom_wireless_metadata.timeout_ms = 30000;  // 30s

        // Write data back
        is_ok = wireless_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_WIRELESS_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Create Wireless handler task
    xTaskCreate(wireless_task, "Wireless Task", 512, NULL, 3, NULL);

    // Register to eeprom save all
    eeprom_register_handler(wireless_config_save);

    return is_ok;
}


bool wireless_config_save() {
    bool is_ok = eeprom_write(EEPROM_WIRELESS_CONFIG_BASE_ADDR, (uint8_t *) &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
    return is_ok;
}


uint32_t get_cyw43_auth(cyw43_auth_t auth) {
    uint32_t cyw43_auth = 0;

    switch (auth) {
        case AUTH_OPEN:
            cyw43_auth = CYW43_AUTH_OPEN;
            break;
        case AUTH_WPA_TKIP_PSK:
            cyw43_auth = CYW43_AUTH_WPA_TKIP_PSK;
            break;
        case AUTH_WPA2_AES_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_AES_PSK;
            break;
        case AUTH_WPA2_MIXED_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_MIXED_PSK;
            break;
        default:
            break;
    }

    return cyw43_auth;
}


void led_interface_task(void *p) {
    wireless_ctrl_t wireless_ctrl;
    uint32_t blink_interval_ms = 0;
    bool prev_led_state = false;
    bool led_state = prev_led_state;

    while (true) {
        prev_led_state = led_state;

        switch (wireless_config.current_wireless_state) {
            case WIRELESS_STATE_NOT_INITIALIZED:
            case WIRELESS_STATE_IDLE:
                blink_interval_ms = 0;
                led_state = false;
                break;
            case WIRELESS_STATE_AP_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_AP_MODE_LISTEN:
                led_state = !led_state;
                blink_interval_ms = 1000;
                break;
            case WIRELESS_STATE_STA_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_STA_MODE_LISTEN:
                led_state = true;
                blink_interval_ms = 0;
                break;
            default:
                break;
        }

        if (led_state != prev_led_state) {
            if (led_state) {
                wireless_ctrl = WIRELESS_CTRL_LED_ON;
            }
            else {
                wireless_ctrl = WIRELESS_CTRL_LED_OFF;
            }
            xQueueSend(wireless_ctrl_queue, &wireless_ctrl, 0);
        }

        
        vTaskDelay(pdMS_TO_TICKS(MAX(blink_interval_ms, LED_INTERFACE_MINIMUM_POLL_PERIOD_MS)));
    }
}


void wireless_task(void *p) {
    static TaskHandle_t led_interface_task_handler = NULL;

    memset(first_line_buffer, 0x0, sizeof(first_line_buffer));
    memset(second_line_buffer, 0x0, sizeof(second_line_buffer));

    wireless_config.current_wireless_state = WIRELESS_STATE_NOT_INITIALIZED;
    wireless_ctrl_queue = xQueueCreate(5, sizeof(wireless_ctrl_t));

    if (cyw43_arch_init()) {
        exit(-1);
    }

    wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;

    // Create LED task
    if (led_interface_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(led_interface_task, "LED Interface Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &led_interface_task_handler);
    }
    else {
        vTaskResume(led_interface_task_handler);
    }

    // Start default initialize pattern
    if (wireless_config.eeprom_wireless_metadata.enable) {
        wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_INIT;
        cyw43_arch_enable_sta_mode();

        // Show the current joining SSID
        sprintf(first_line_buffer, ">%s", wireless_config.eeprom_wireless_metadata.ssid);

        // If the authentication method is open then the password is NULL
        const char * wifi_password = NULL;
        if (wireless_config.eeprom_wireless_metadata.auth != AUTH_OPEN) {
            wifi_password = wireless_config.eeprom_wireless_metadata.pw;
        }

        // Retry within timeframe
        TickType_t stop_tick = xTaskGetTickCount() + pdMS_TO_TICKS(wireless_config.eeprom_wireless_metadata.timeout_ms);
        while (xTaskGetTickCount() < stop_tick) {
            int resp;
            resp = cyw43_arch_wifi_connect_timeout_ms(wireless_config.eeprom_wireless_metadata.ssid,
                                                      wifi_password,
                                                      get_cyw43_auth(wireless_config.eeprom_wireless_metadata.auth),
                                                      wireless_config.eeprom_wireless_metadata.timeout_ms);
            if (resp == PICO_OK) {
                wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
                break;
            }
            else {

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
    // If the current wireless state is AP mode then we will map / to the wifi configuration
    rest_endpoints_init(wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN);

    // Start the HTTP server
    httpd_init();

    while (true) {
        wireless_ctrl_t wireless_ctrl;

        xQueueReceive(wireless_ctrl_queue, &wireless_ctrl, portMAX_DELAY);

        switch (wireless_ctrl) {
            case WIRELESS_CTRL_LED_ON:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                break;
            case WIRELESS_CTRL_LED_OFF:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                break;
            default:
                break;
        }
    }

    cyw43_arch_deinit();

    vTaskSuspend(led_interface_task_handler);
}


uint8_t wireless_view_wifi_info(void) {
    static TaskHandle_t wirelss_info_render_task_handler = NULL;
    if (wirelss_info_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(wirelss_info_render_task, "Wireless Display Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &wirelss_info_render_task_handler);
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
    // Mapping
    // w0 (str): ssid
    // w1 (str): pw
    // w2 (int): auth
    // w3 (int): timeout_ms
    // w4 (bool): enable
    // ee (bool): save to eeprom

    static char wireless_config_json_buffer[256];
    bool save_to_eeprom = false;

    // If the argument includes control, then update the settings
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "w0") == 0) {
            strncpy(wireless_config.eeprom_wireless_metadata.ssid, values[idx], sizeof(wireless_config.eeprom_wireless_metadata.ssid)); 
        }
        else if (strcmp(params[idx], "w1") == 0) {
            strncpy(wireless_config.eeprom_wireless_metadata.pw, values[idx], sizeof(wireless_config.eeprom_wireless_metadata.pw)); 
        }
        else if (strcmp(params[idx], "w2") == 0) {
            cyw43_auth_t auth = (cyw43_auth_t) atoi(values[idx]);
            wireless_config.eeprom_wireless_metadata.auth = auth;
        }
        else if (strcmp(params[idx], "w3") == 0) {
            int timeout_ms = (uint16_t) atoi(values[idx]);
            wireless_config.eeprom_wireless_metadata.timeout_ms = timeout_ms;
        }
        else if (strcmp(params[idx], "w4") == 0) {
            bool enable = string_to_boolean(values[idx]);
            wireless_config.eeprom_wireless_metadata.enable = enable;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        wireless_config_save();
    }

    // Response
    snprintf(wireless_config_json_buffer, 
             sizeof(wireless_config_json_buffer),
             "%s"
             "{\"w0\":\"%s\",\"w2\":%d,\"w3\":%"PRId32",\"w4\":%s}",
             http_json_header,
             wireless_config.eeprom_wireless_metadata.ssid,
            //  wireless_config.eeprom_wireless_metadata.pw,  // No, we don't send the password over anymore
             wireless_config.eeprom_wireless_metadata.auth,
             wireless_config.eeprom_wireless_metadata.timeout_ms,
             boolean_to_string(wireless_config.eeprom_wireless_metadata.enable));

    size_t data_length = strlen(wireless_config_json_buffer);
    file->data = wireless_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}