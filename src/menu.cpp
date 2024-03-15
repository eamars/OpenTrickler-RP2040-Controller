#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include <u8g2.h>
#include <mui.h>
#include <mui_u8g2.h>

#include "app.h"
#include "configuration.h"
#include "rotary_button.h"
#include "scale.h"
#include "display.h"
#include "rotary_button.h"
#include "eeprom.h"
#include "charge_mode.h"
#include "cleanup_mode.h"
#include "eeprom.h"
#include "wireless.h"


// External variables
extern muif_t muif_list[];
extern fds_t fds_data[];
extern const size_t muif_cnt;

// External menus

// Local variables

AppState_t exit_state = APP_STATE_DEFAULT;


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

    // Flags
    bool charge_mode_skip_user_input = false;  // Used to skip user input if the charge mode weight is received from REST

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
            else if (button_encoder_event == OVERRIDE_FROM_REST) {
                // Assuming the caller code will set the exit_state
                mui_SaveForm(&mui);          // store the current form and position so that the child can jump back
                mui_LeaveForm(&mui);
                charge_mode_skip_user_input = true;
            }
        }
        else {
            uint8_t exit_form_id = 1;  // by default it goes to the main menu
            // menu is not active, leave the control to the app
            switch (exit_state) {
                case APP_STATE_ENTER_CHARGE_MODE:
                    exit_form_id = charge_mode_menu(charge_mode_skip_user_input);
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
