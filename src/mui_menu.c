/*
  This file is created to be compiled in C instead of C++ mode. 
*/
#include <stdio.h>
#include "u8g2.h"
#include "mui.h"
#include "mui_u8g2.h"
#include "app.h"
#include "pico/stdlib.h"

#include "scale.h"
#include "charge_mode.h"
#include "version.h"
#include "common.h"
#include "profile.h"
#include "servo_gate.h"


// External modules/varaibles
extern uint8_t charge_weight_digits[];
extern AppState_t exit_state;
extern charge_mode_config_t charge_mode_config;
extern servo_gate_t servo_gate;
extern scale_config_t scale_config;
extern eeprom_profile_data_t profile_data;


const char * get_selected_profile_name(void * data, uint16_t idx) {
    return profile_data.profiles[idx].name;
}

uint16_t get_profile_count() {
    return MAX_PROFILE_CNT;
}


uint8_t mui_hrule(mui_t *ui, uint8_t msg)
{
    u8g2_t *u8g2 = mui_get_U8g2(ui);
    switch(msg)
    {
        case MUIF_MSG_DRAW:
            u8g2_DrawHLine(u8g2, 0, mui_get_y(ui), u8g2_GetDisplayWidth(u8g2));
            break;
    }
    return 0;
}


uint8_t render_version_page(mui_t * ui, uint8_t msg) {
    switch (msg) {
        case MUIF_MSG_DRAW:
        {
            u8g2_uint_t x = mui_get_x(ui);
            u8g2_uint_t y = mui_get_y(ui);
            u8g2_t *u8g2 = mui_get_U8g2(ui);

            char buf[32];

            u8g2_SetFont(u8g2, u8g2_font_profont11_tf);

            snprintf(buf, sizeof(buf), "Ver: %s", version_string);
            u8g2_DrawStr(u8g2, x, y, buf);

            snprintf(buf, sizeof(buf), "VCS: %s", vcs_hash);
            u8g2_DrawStr(u8g2, x, y + 10, buf);

            snprintf(buf, sizeof(buf), "Build: %s", build_type);
            u8g2_DrawStr(u8g2, x, y + 20, buf);

            break;
        }
        default:
            break;
    }
    return 0;
}


uint8_t render_profile_ver_info(mui_t *ui, uint8_t msg) {
    switch (msg) {
        case MUIF_MSG_DRAW: 
        {
            u8g2_uint_t x = mui_get_x(ui);
            u8g2_uint_t y = mui_get_y(ui);
            u8g2_t *u8g2 = mui_get_U8g2(ui);

            u8g2_SetFont(u8g2, u8g2_font_profont11_tf);

            profile_t * current_profile = profile_get_selected();

            char buf[32];
            snprintf(buf, sizeof(buf), 
                     "Rev:%lx,Comp:%lx", current_profile->rev, current_profile->compatibility);

            u8g2_DrawStr(u8g2, x, y, buf);
        }
    }

    return 0;
}


uint8_t render_profile_pid_details(mui_t *ui, uint8_t msg) {
    switch(msg)
    {
        case MUIF_MSG_DRAW:
        {
            char buf[30];
            u8g2_t *u8g2 = mui_get_U8g2(ui);
            profile_t * current_profile = profile_get_selected();

            // Render Coarse
            u8g2_SetFont(u8g2, u8g2_font_profont11_tf);
            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Kp:%0.3f", current_profile->coarse_kp);
            u8g2_DrawStr(u8g2, 5, 25, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Ki:%0.3f", current_profile->coarse_ki);
            u8g2_DrawStr(u8g2, 5, 35, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Kd:%0.3f", current_profile->coarse_kd);
            u8g2_DrawStr(u8g2, 5, 45, buf);

            // Render fine
            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Kp:%0.3f", current_profile->fine_kp);
            u8g2_DrawStr(u8g2, 65, 25, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Ki:%0.3f", current_profile->fine_ki);
            u8g2_DrawStr(u8g2, 65, 35, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Kd:%0.3f", current_profile->fine_kd);
            u8g2_DrawStr(u8g2, 65, 45, buf);
            break;
        }            
    }
    return 0;
}


uint8_t render_charge_mode_next_button(mui_t * ui, uint8_t msg) {
    switch (msg) {
        case MUIF_MSG_CURSOR_SELECT:
        case MUIF_MSG_VALUE_INCREMENT:
        case MUIF_MSG_VALUE_DECREMENT:
            mui_SaveForm(ui);
            if (charge_mode_config.eeprom_charge_mode_data.decimal_places == DP_2) {
                ui->arg = 11;  // goto form 11
            }
            else if (charge_mode_config.eeprom_charge_mode_data.decimal_places == DP_3) {
                ui->arg = 12;  // goto form 12
            }
            return mui_GotoFormAutoCursorPosition(ui, ui->arg);
        default:
            mui_u8g2_btn_goto_wm_fi(ui, msg);
            break;
    }

    return 0;
}


uint8_t render_profile_misc_details(mui_t *ui, uint8_t msg) {
    switch(msg)
    {
        case MUIF_MSG_DRAW:
        {
            char buf[30];
            u8g2_t *u8g2 = mui_get_U8g2(ui);
            profile_t * current_profile = profile_get_selected();

            // Render speed
            u8g2_SetFont(u8g2, u8g2_font_profont11_tf);
            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Coarse:%0.3f,%0.3f", current_profile->coarse_min_flow_speed_rps, current_profile->coarse_max_flow_speed_rps);
            u8g2_DrawStr(u8g2, 5, 25, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "Fine  :%0.3f,%0.3f", current_profile->fine_min_flow_speed_rps, current_profile->fine_max_flow_speed_rps);
            u8g2_DrawStr(u8g2, 5, 35, buf);

            break;
        }            
    }
    return 0;
}


uint8_t render_servo_gate_state_with_action(mui_t *ui, uint8_t msg) {
    uint8_t return_value = mui_u8g2_u8_radio_wm_pi(ui, msg);

    switch (msg) {
        case MUIF_MSG_CURSOR_SELECT:
        {
            uint8_t *value = (uint8_t *)muif_get_data(ui->uif);
            gate_state_t state = (gate_state_t)(*value);

            float ratio = SERVO_GATE_RATIO_DISABLED;

            switch (state) {
                case GATE_OPEN:
                    ratio = SERVO_GATE_RATIO_OPEN;
                    break;

                case GATE_CLOSE:
                    ratio = SERVO_GATE_RATIO_CLOSED;
                    break;

                case GATE_DISABLED:
                default:
                    ratio = SERVO_GATE_RATIO_DISABLED;
                    break;
            }

            servo_gate_set_ratio(ratio, false);
            break;
        }
    }

    return return_value;
}



muif_t muif_list[] = {
        /* normal text style */
        MUIF_U8G2_FONT_STYLE(0, u8g2_font_helvR08_tr),

        /* bold text style */
        MUIF_U8G2_FONT_STYLE(1, u8g2_font_helvB08_tr),

        /* monospaced font */
        MUIF_U8G2_FONT_STYLE(2, u8g2_font_profont12_tr),

        // Large mono space font
        MUIF_U8G2_FONT_STYLE(3, u8g2_font_profont22_tf),

        MUIF_U8G2_LABEL(),                                                    /* allow MUI_LABEL command */

        // Horizontal line
        MUIF_RO("HL", mui_hrule),

        /* main menu */
        MUIF_RO("MU",mui_u8g2_goto_data),
        MUIF_BUTTON("GC", mui_u8g2_goto_form_w1_pi),

        /* Goto Form Button where the width is equal to the size of the text, spaces can be used to extend the size */
        MUIF_BUTTON("BN", mui_u8g2_btn_goto_wm_fi),

        MUIF_BUTTON("B1", render_charge_mode_next_button),

        // Leave
        MUIF_VARIABLE("LV", &exit_state, mui_u8g2_btn_exit_wm_fi),

        // Scale driver selection
        MUIF_VARIABLE("SD", &scale_config.persistent_config.scale_driver, mui_u8g2_u8_opt_line_wa_mud_pi),

        // Baud rate selection
        MUIF_VARIABLE("BR", &scale_config.persistent_config.scale_baudrate, mui_u8g2_u8_opt_line_wa_mud_pi),

        // Render version
        MUIF_RO("VE", render_version_page),

        // Render servo gate state
        MUIF_VARIABLE("RB",&servo_gate.gate_state, render_servo_gate_state_with_action),

        // input for a number between 0 to 9 //
        MUIF_U8G2_U8_MIN_MAX("N4", &charge_weight_digits[4], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N3", &charge_weight_digits[3], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N2", &charge_weight_digits[2], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N1", &charge_weight_digits[1], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N0", &charge_weight_digits[0], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),

        MUIF_U8G2_U16_LIST("P0", (uint16_t *) &profile_data.current_profile_idx, NULL, get_selected_profile_name, get_profile_count, mui_u8g2_u16_list_parent_wm_pi),
        MUIF_U8G2_U16_LIST("P1", (uint16_t *) &profile_data.current_profile_idx, NULL, get_selected_profile_name, get_profile_count, mui_u8g2_u16_list_child_w1_pi),

        // Render profile details
        MUIF_RO("P2", render_profile_ver_info),
        MUIF_RO("P3", render_profile_pid_details),
        MUIF_RO("P4", render_profile_misc_details)
    };

const size_t muif_cnt = sizeof(muif_list) / sizeof(muif_t);

fds_t fds_data[] = {
    // Main menu
    MUI_FORM(1)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "OpenTrickler")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU", 
        MUI_10 "Start|"
        MUI_20 "Cleanup|"
        MUI_40 "Wireless|"
        MUI_30 "Settings"
        )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)

    // Menu 10: Select profile
    MUI_FORM(10)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Profile")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_XYT("B1",115, 59, "Next")  // Jump to form 11 or 12
    MUI_XYAT("BN",14, 59, 1, "Back")  // Jump to form 1
    MUI_XYA("P0", 5, 25, 33)  // Jump to form 33 (profile selection)

    // Menu 11: Charge Weight (2dp)
    MUI_FORM(11)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Charge Weight")
    MUI_XY("HL", 0,13)

    MUI_STYLE(3)
    MUI_XY("N3",36, 35)
    MUI_XY("N2",52, 35)
    MUI_LABEL(64, 35, ".")
    MUI_XY("N1",76, 35)
    MUI_XY("N0",92, 35)

    MUI_STYLE(0)
    MUI_XY("SU", 106, 35)

    MUI_STYLE(0)
    MUI_XYAT("BN",115, 59, 13, "Next")
    MUI_XYAT("BN",14, 59, 10, "Back")

    MUI_STYLE(3)
    MUI_XY("N4",20, 35)

    // Menu 12: Charge Weight (3dp)
    MUI_FORM(12)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Charge Weight")
    MUI_XY("HL", 0,13)

    MUI_STYLE(3)
    MUI_XY("N3",36, 35)
    MUI_LABEL(48, 35, ".")
    MUI_XY("N2",60, 35)
    MUI_XY("N1",76, 35)
    MUI_XY("N0",92, 35)

    MUI_STYLE(0)
    MUI_XY("SU", 106, 35)

    MUI_STYLE(0)
    MUI_XYAT("BN",115, 59, 13, "Next")
    MUI_XYAT("BN",14, 59, 10, "Back")

    MUI_STYLE(3)
    MUI_XY("N4",20, 35)

    // Menu 13: Warning page
    MUI_FORM(13)
    MUI_STYLE(1)
    MUI_LABEL(5, 10, "Warning")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Put pan on the scale and")
    MUI_LABEL(5, 37, "press Next to trickle")

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 10, "Back")
    MUI_XYAT("LV", 115, 59, 1, "Next")  // APP_STATE_ENTER_CHARGE_MODE

    // Menu 20: Cleanup
    MUI_FORM(20)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Warning")
    MUI_XY("HL", 0,13)
    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Put pan on the scale and")
    MUI_LABEL(5, 37, "press Next to cleanup")
    MUI_XYAT("BN",14, 59, 1, "Back")
    MUI_XYAT("LV", 115, 59, 5, "Next")  // APP_STATE_ENTER_CLEANUP_MODE

    // Menu 30: Configurations
    MUI_FORM(30)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Settings")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU", 
        MUI_31 "Scale|"
        MUI_32 "Profile Manager|"
        MUI_37 "EEPROM|"
        MUI_39 "Servo Gate|"
        MUI_35 "Reboot|"
        MUI_36 "Version|"
        MUI_1 "<-Return"  // Back to main menu
        )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)

    // Menu 31: Scale (main menu)
    MUI_FORM(31)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Scale")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU",
        MUI_53 "Select Driver|"
        MUI_51 "Calibration|"
        MUI_30 "<-Return"  // back to view 30
    )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)
    
    // Menu 32: Select Profile Page
    MUI_FORM(32)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Profile")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_XYA("P0", 5, 25, 33)  // Jump to form 33
    MUI_XYAT("BN",115, 59, 34, "Next")  // Jump to form 34
    MUI_XYAT("BN",14, 59, 30, "Back")  // Jump to form 30

    // Render details
    MUI_XY("P2", 5, 37)

    // Child List for profile selection
    MUI_FORM(33)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Profile")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_XYA("P1", 5, 25, 0) 
    MUI_XYA("P1", 5, 37, 1) 
    MUI_XYA("P1", 5, 49, 2) 
    MUI_XYA("P1", 5, 61, 3)

    // Menu 34: profile details (PID)
    MUI_FORM(34)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Profile Details (1/2)")
    MUI_XY("HL", 0,13)

    // Draw details
    MUI_AUX("P3")

    MUI_STYLE(0)
    MUI_XYAT("BN",115, 59, 38, "Next")  // Jump next to page 38
    MUI_XYAT("BN",14, 59, 32, "Back")  // Jump back to form 30

    // Menu 38: profile details (others)
    MUI_FORM(38)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Profile Details (2/2)")
    MUI_XY("HL", 0,13)

    // Draw details
    MUI_AUX("P4")

    MUI_STYLE(0)
    MUI_XYAT("BN",115, 59, 30, "Exit")  // Jump back to form 30
    MUI_XYAT("BN",14, 59, 34, "Back")  // Jump next to page 30


    // Menu 35: Reboot
    MUI_FORM(35)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Reboot")
    MUI_XY("HL", 0,13)
    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Press Next to perform")
    MUI_LABEL(5, 37, "software reboot")
    MUI_XYAT("BN",14, 59, 30, "Back")
    MUI_XYAT("LV", 115, 59, 9, "Next")  // APP_STATE_ENTER_REBOOT

    // Menu 36 Version
    MUI_FORM(36)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Version Info")
    MUI_XY("HL", 0,13)

    MUI_XY("VE", 5, 25)  // Version text

    MUI_STYLE(0)
    MUI_XYAT("BN", 64, 59, 30, " OK ")

    // EEPROM submenu
    MUI_FORM(37)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "EEPROM")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU",
        MUI_60 "Save to EEPROM|"
        MUI_61 "Erase EEPROM|"
        MUI_30 "<-Return"  // back to view 30
    )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)

    // Servo Gate submenu
    MUI_FORM(39)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Servo Gate Control")
    MUI_XY("HL", 0,13)

    MUI_XYAT("RB", 5, 25, 0, "Disable")
    MUI_XYAT("RB", 5, 37, 1, "Close")
    MUI_XYAT("RB", 5, 49, 2, "Open")
    MUI_XYAT("BN", 64, 59, 30, " OK ")  // Jump to form 30

    // Wirelss submenu
    MUI_FORM(40)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Wireless")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU",
        MUI_41 "Wifi Info|"
        MUI_1 "<-Return"  // back to view 1
    )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)

    // Wifi info
    MUI_FORM(41)
    MUI_LABEL(5,10, "Wifi Info")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(3,27, "Press OK to view Wifi")
    MUI_LABEL(3,37, "information")

    MUI_STYLE(0)
    MUI_XYAT("LV",64, 59, 10, " OK ")  // APP_STATE_ENTER_WIFI_INFO


    // Scale calibration
    MUI_FORM(51)
    MUI_STYLE(1)
    MUI_LABEL(5, 10, "Warning")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Clear the scale and press")
    MUI_LABEL(5, 37, "Next to calibrate")

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 31, "Back")
    MUI_XYAT("LV", 115, 59, 6, "Next")  // APP_STATE_ENTER_SCALE_CALIBRATION

    // Scale driver
    MUI_FORM(53)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Scale Driver")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5,25, "Driver:")
    MUI_XYAT("SD", 50, 25, 60, "A&D FX-i Std|Steinberg SBS|G&G JJB|US Solid JFDBS|JM Science|Creedmoor|Radwag PS R2|Sartorius|Generic")

    MUI_LABEL(5,37, "Baudrate:")
    MUI_XYAT("BR", 50, 37, 60, "4800|9600|19200")

    MUI_STYLE(0)
    MUI_XYAT("BN", 64, 59, 31, " OK ")

    // Save to EEPROM
    MUI_FORM(60)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Save to EEPROM")
    MUI_XY("HL", 0,13)
    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Press Next to save")
    MUI_LABEL(5, 37, "changes to EEPROM")
    MUI_XYAT("BN",14, 59, 37, "Back")
    MUI_XYAT("LV", 115, 59, 7, "Next")  // APP_STATE_ENTER_EEPROM_SAVE

    // Erase entire EEPROM
    MUI_FORM(61)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Erase EEPROM")
    MUI_XY("HL", 0,13)
    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Press Next to erase")
    MUI_LABEL(5, 37, "the EEPROM")
    MUI_XYAT("BN",14, 59, 37, "Back")
    MUI_XYAT("LV", 115, 59, 8, "Next")  // APP_STATE_ENTER_EEPROM_ERASE
};