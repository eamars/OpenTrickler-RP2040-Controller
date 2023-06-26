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


extern uint8_t charge_weight_digits[];
extern AppState_t exit_state;
extern charge_mode_config_t charge_mode_config;

// Imported from and_scale module
extern eeprom_scale_data_t scale_data;



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


uint8_t render_pid_values(mui_t *ui, uint8_t msg, float kp, float ki, float kd) {
    u8g2_t *u8g2 = mui_get_U8g2(ui);
    switch(msg)
    {
        case MUIF_MSG_DRAW:
        {
            char buf[30];

            // Render text
            u8g2_SetFont(u8g2, u8g2_font_profont11_tf);
            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "KP:%0.2f", kp);
            u8g2_DrawStr(u8g2, 5, 25, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "KI:%0.2f", ki);
            u8g2_DrawStr(u8g2, 5, 35, buf);

            memset(buf, 0x0, sizeof(buf));
            snprintf(buf, sizeof(buf), "KD:%0.2f", kd);
            u8g2_DrawStr(u8g2, 5, 45, buf);
            break;
        }            
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
        break;
    }
    return 0;
}


uint8_t render_coarse_pid_values(mui_t *ui, uint8_t msg) {
    return render_pid_values(ui, msg, 
                             charge_mode_config.eeprom_charge_mode_data.coarse_kp, 
                             charge_mode_config.eeprom_charge_mode_data.coarse_ki, 
                             charge_mode_config.eeprom_charge_mode_data.coarse_kd);
}

uint8_t render_fine_pid_values(mui_t *ui, uint8_t msg) {
    return render_pid_values(ui, msg, 
                             charge_mode_config.eeprom_charge_mode_data.fine_kp, 
                             charge_mode_config.eeprom_charge_mode_data.fine_ki, 
                             charge_mode_config.eeprom_charge_mode_data.fine_kd);
}

uint8_t render_scale_unit(mui_t * ui, uint8_t msg) {
    switch (msg) {
        case MUIF_MSG_DRAW:
        {
            u8g2_uint_t x = mui_get_x(ui);
            u8g2_uint_t y = mui_get_y(ui);
            u8g2_t *u8g2 = mui_get_U8g2(ui);

            u8g2_SetFont(u8g2, u8g2_font_helvR08_tr);
            u8g2_DrawStr(u8g2, x, y, get_scale_unit_string(true));
        }
        break;
    }
    return 0;
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

        // Leave
        MUIF_VARIABLE("LV", &exit_state, mui_u8g2_btn_exit_wm_fi),

        // Unit selection
        MUIF_VARIABLE("UN",&scale_data.scale_unit, mui_u8g2_u8_opt_line_wa_mud_pi),

        // Render unit
        MUIF_RO("SU", render_scale_unit),

        // Render version
        MUIF_RO("VE", render_version_page),

        // input for a number between 0 to 9 //
        MUIF_U8G2_U8_MIN_MAX("N4", &charge_weight_digits[4], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N3", &charge_weight_digits[3], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N2", &charge_weight_digits[2], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N1", &charge_weight_digits[1], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N0", &charge_weight_digits[0], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),

        // Render PID value views
        MUIF_RO("K0", render_coarse_pid_values),
        MUIF_RO("K1", render_fine_pid_values),

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

    // Menu 10: Start
    MUI_FORM(10)
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
    // MUI_LABEL(106, 35, "gr")  // TODO: Make it variable

    MUI_STYLE(0)
    MUI_XYAT("BN",115, 59, 11, "Next")
    MUI_XYAT("BN",14, 59, 1, "Back")

    MUI_STYLE(3)
    MUI_XY("N4",20, 35)


    // Menu 11: 
    MUI_FORM(11)
    MUI_STYLE(1)
    MUI_LABEL(5, 10, "Warning")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Put pan on the scale and")
    MUI_LABEL(5, 37, "press Next to trickle")

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 10, "Back")
    MUI_XYAT("LV", 115, 59, 1, "Next")  // APP_STATE_ENTER_CHARGE_MODE
    // MUI_XYAT("BN",115, 59, 0, "Next")

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
        MUI_32 "View PID|"
        MUI_34 "Tune PID|"
        MUI_37 "EEPROM|"
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
        MUI_50 "Select Unit|"
        MUI_51 "Calibration|"
        MUI_52 "Enable Fast Report|"
        MUI_30 "<-Return"  // back to view 30
    )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)
    
    // Menu 32: View PID page 1
    MUI_FORM(32)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Coarse Trickler PID")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 30, "Back")
    MUI_XYAT("BN", 115, 59, 33, "Next")  // APP_STATE_ENTER_CHARGE_MODE
    MUI_AUX("K0")

    // Menu 33: View PID page 2
    MUI_FORM(33)

    MUI_STYLE(1)
    MUI_LABEL(5,10, "Fine Trickler PID")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 32, "Back")
    MUI_XYAT("BN", 115, 59, 30, " OK ")  // APP_STATE_ENTER_CHARGE_MODE
    MUI_AUX("K1")

    // Menu 35: Reboot
    MUI_FORM(35)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Reboot")
    MUI_XY("HL", 0,13)
    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Press Next to perform")
    MUI_LABEL(5, 37, "software reboot")
    MUI_XYAT("BN",14, 59, 37, "Back")
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

    // Wirelss submenu
    MUI_FORM(40)
#ifdef RASPBERRYPI_PICO_W
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
#else  // doesn't support wifi

    MUI_STYLE(1)
    MUI_LABEL(5,10, "Error")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(3,27, "AP Mode is not supported")
    MUI_LABEL(3,37, "on your platform")

    MUI_STYLE(0)
    MUI_XYAT("BN",64, 59, 1, " OK ")
#endif

    // Wifi info
    MUI_FORM(41)
    MUI_LABEL(5,10, "Wifi Info")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(3,27, "Press OK to view Wifi")
    MUI_LABEL(3,37, "information")

    MUI_STYLE(0)
    MUI_XYAT("LV",64, 59, 10, " OK ")  // APP_STATE_ENTER_WIFI_INFO

    
    // Scale unit
    MUI_FORM(50)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Unit")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5,25, "Unit:")
    MUI_XYAT("UN", 60, 25, 60, "Grain (gn)|Gram (g)")

    MUI_STYLE(0)
    MUI_XYAT("BN", 64, 59, 31, " OK ")

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