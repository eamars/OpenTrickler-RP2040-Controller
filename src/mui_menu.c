/*
  This file is created to be compiled in C instead of C++ mode. 
*/
#include <stdio.h>
#include "u8g2.h"
#include "mui.h"
#include "mui_u8g2.h"
#include "app.h"


extern uint8_t charge_weight_digits[];
extern AppState_t exit_state;
extern MotorControllerSelect_t coarse_motor_controller_select;
extern MotorControllerSelect_t fine_motor_controller_select;

MeasurementUnit_t measurement_unit;

float coarse_kp = 4.5f;
float coarse_ki = 0.0f;
float coarse_kd = 150.0f;
char coarse_kp_string[20];

float fine_kp = 200.0f;
float fine_ki = 0.0f;
float fine_kd = 150.0f;


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

uint8_t render_coarse_pid_values(mui_t *ui, uint8_t msg) {
    return render_pid_values(ui, msg, coarse_kp, coarse_ki, coarse_kd);
}

uint8_t render_fine_pid_values(mui_t *ui, uint8_t msg) {
    return render_pid_values(ui, msg, fine_kp, fine_ki, fine_kd);
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
        MUIF_VARIABLE("UN",&measurement_unit, mui_u8g2_u8_opt_line_wa_mud_pi),

        // motor controller selection
        MUIF_VARIABLE("MC",&coarse_motor_controller_select, mui_u8g2_u8_opt_line_wa_mud_pi),
        MUIF_VARIABLE("MF",&fine_motor_controller_select, mui_u8g2_u8_opt_line_wa_mud_pi),

        /* input for a number between 0 to 9 */
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
        MUI_30 "Configurations"
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
    MUI_XY("N3",26, 35)
    MUI_XY("N2",42, 35)
    MUI_LABEL(54, 35, ".")
    MUI_XY("N1",66, 35)
    MUI_XY("N0",82, 35)

    MUI_STYLE(0)
    MUI_LABEL(96, 35, "gr")

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 1, "Back")
    MUI_XYAT("BN",115, 59, 11, "Next")


    // Menu 11: 
    MUI_FORM(11)
    MUI_STYLE(1)
    MUI_LABEL(5, 10, "Ready?")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5, 25, "Put pan on the scale and")
    MUI_LABEL(5, 37, "press Next to trickle")

    MUI_STYLE(0)
    MUI_XYAT("BN",14, 59, 10, "Back")
    MUI_XYAT("LV", 115, 59, 1, "Next")  // APP_STATE_ENTER_CHARGE_MODE
    // MUI_XYAT("BN",115, 59, 0, "Next")

    // Menu 20: Cleanup
    // MUI_FORM(20)
    // MUI_STYLE(1)
    // MUI_LABEL(5,10, "Select Cleanup")
    // MUI_XY("HL", 0,13)

    // Menu 30: Configurations
    MUI_FORM(30)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Configurations")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("MU", 
        MUI_31 "Unit|"
        MUI_32 "View PID|"
        MUI_34 "Tune PID|"
        MUI_35 "Motor Controller|"
        MUI_1 "<-Return"  // Back to main menu
        )
    MUI_XYA("GC", 5, 25, 0) 
    MUI_XYA("GC", 5, 37, 1) 
    MUI_XYA("GC", 5, 49, 2) 
    MUI_XYA("GC", 5, 61, 3)

    // Menu 31: Select Unit
    MUI_FORM(31)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Unit")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5,25, "Unit:")
    MUI_XYAT("UN", 60, 25, 60, "Grain (gr)|Gram (g)")

    MUI_STYLE(0)
    MUI_XYAT("BN", 64, 59, 30, " OK ")

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

    // Menu 35: Select motor controller
    MUI_FORM(35)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Motor Controller")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_LABEL(5,27, "Coarse:")
    MUI_XYAT("MC", 60, 27, 60, "TMC2209|TMC2130|TMC5160")

    MUI_LABEL(5,43, "Fine:")
    MUI_XYAT("MF", 60, 43, 60, "TMC2209|TMC2130|TMC5160")

    MUI_STYLE(0)
    MUI_XYAT("BN",64, 59, 30, " OK ")
};