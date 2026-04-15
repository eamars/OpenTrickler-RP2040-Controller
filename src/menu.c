#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <u8g2.h>
#include <mui.h>
#include <mui_u8g2.h>

#include "app.h"
#include "configuration.h"
#include "scale.h"
#include "display.h"
#include "mini_12864_module.h"
#include "eeprom.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "eeprom.h"
#include "wireless.h"
#include "system_control.h"

// External variables
extern muif_t muif_list[];
extern fds_t fds_data[];
extern const size_t muif_cnt;

// External menus

// Local variables

AppState_t exit_state = APP_STATE_DEFAULT;

static uint8_t menu_get_parent_form_id(uint8_t form_id) {
    // Parent relationships for "one level up" navigation.
    // Root is form 1.
    switch (form_id) {
        case 10: return 1;
        case 11: return 10;
        case 12: return 10;
        case 13: return 10;

        case 20: return 1;

        case 30: return 1;
        case 31: return 30;
        case 32: return 30;
        case 33: return 32;
        case 34: return 32;
        case 38: return 34;
        case 35: return 30;
        case 36: return 30;
        case 37: return 30;
        case 39: return 30;

        case 40: return 1;
        case 41: return 40;

        case 51: return 31;
        case 53: return 31;
        case 60: return 37;
        case 61: return 37;

        default:
            return 1;
    }
}


void menu_task(void *p){
    u8g2_t * display_handler = get_display_handler();
    // Create UI element
    mui_t mui;

    mui_Init(&mui, display_handler, fds_data, muif_list, muif_cnt);
    mui_GotoForm(&mui, 1, 0);

    // Render the menu before user input
    u8g2_ClearBuffer(display_handler);
    mui_Draw(&mui);
    u8g2_SendBuffer(display_handler);

    while (true) {
        if (mui_IsFormActive(&mui)) {
            // Block wait for the user input
            ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(true);
            if (button_encoder_event == BUTTON_ENCODER_ROTATE_CW) {
                mui_NextField(&mui);
            }
            else if (button_encoder_event == BUTTON_ENCODER_ROTATE_CCW) {
                mui_PrevField(&mui);
            }
            else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
                mui_SendSelect(&mui);
            }
            else if (button_encoder_event == BUTTON_RST_PRESSED) {
                // Global back: deterministic "one menu level up" until root (form 1).
                uint8_t current_form_id = mui_GetCurrentFormId(&mui);
                if (current_form_id != 1) {
                    uint8_t parent_form_id = menu_get_parent_form_id(current_form_id);
                    mui_GotoForm(&mui, parent_form_id, 0);
                }
            }
            else if (button_encoder_event == OVERRIDE_FROM_REST) {
                // Assuming the caller code will set the exit_state
                mui_SaveForm(&mui);          // store the current form and position so that the child can jump back
                mui_LeaveForm(&mui);
            }
        }
        else {
            uint8_t exit_form_id = 1;  // by default it goes to the main menu
            // menu is not active, leave the control to the app
            switch (exit_state) {
                case APP_STATE_ENTER_CHARGE_MODE:
                    exit_form_id = charge_mode_menu(false);
                    break;
                case APP_STATE_ENTER_CHARGE_MODE_FROM_REST:
                    exit_form_id = charge_mode_menu(true);
                    break;
                case APP_STATE_ENTER_CLEANUP_MODE:
                    exit_form_id = cleanup_mode_menu();
                    break;
                case APP_STATE_ENTER_SCALE_CALIBRATION:
                    exit_form_id = scale_calibrate_with_external_weight();
                    break;
                case APP_STATE_ENTER_EEPROM_SAVE: 
                    exit_form_id = eeprom_save_all();
                    break;
                case APP_STATE_ENTER_EEPROM_ERASE:
                    exit_form_id = eeprom_erase(true);
                    break;
                case APP_STATE_ENTER_REBOOT:
                    exit_form_id = software_reboot();
                    break;
                case APP_STATE_ENTER_WIFI_INFO:
                    exit_form_id = wireless_view_wifi_info();
                    break;
                default:
                    break;
            }

            mui_GotoForm(&mui, exit_form_id, 0);
        }

        u8g2_ClearBuffer(display_handler);
        mui_Draw(&mui);
        u8g2_SendBuffer(display_handler);
    }
}
