/*
  This file is created to be compiled in C instead of C++ mode. 
*/

#include "u8g2.h"
#include "mui.h"
#include "mui_u8g2.h"
#include "app.h"


extern uint8_t charge_weight_digits[];
extern AppState_t exit_state;


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
        MUIF_RO("M0",mui_u8g2_goto_data),
        MUIF_BUTTON("GC", mui_u8g2_goto_form_w1_pi),

        /* Goto Form Button where the width is equal to the size of the text, spaces can be used to extend the size */
        MUIF_BUTTON("BN", mui_u8g2_btn_goto_wm_fi),

        // Leave
        MUIF_VARIABLE("LV", &exit_state, mui_u8g2_btn_exit_wm_fi),

        /* input for a number between 0 to 9 */
        MUIF_U8G2_U8_MIN_MAX("N3", &charge_weight_digits[3], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N2", &charge_weight_digits[2], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N1", &charge_weight_digits[1], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
        MUIF_U8G2_U8_MIN_MAX("N0", &charge_weight_digits[0], 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    };

const size_t muif_cnt = sizeof(muif_list) / sizeof(muif_t);

fds_t fds_data[] = {
    // Main menu
    MUI_FORM(0)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "OpenTrickler")
    MUI_XY("HL", 0,13)

    MUI_STYLE(0)
    MUI_DATA("M0", 
        MUI_10 "Start|"
        MUI_20 "Cleanup|"
        MUI_30 "Configuration"
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
    MUI_XYAT("BN",14, 59, 0, "Back")
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
    MUI_FORM(20)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Select Cleanup")
    MUI_XY("HL", 0,13)

    // Menu 3: Configuration
    MUI_FORM(30)
    MUI_STYLE(1)
    MUI_LABEL(5,10, "Configuration")
    MUI_XY("HL", 0,13)
};