#ifndef LV_CONF_H
#define LV_CONF_H

/*
    LVGL for the mini 12864 (ADR-010, gate 4): a 128 x 64 one-bit screen of labels, drawn with the menu's own
    styles and fonts (menu_view_12864.c, menu_fonts.c). Trimmed as ADR-010's gate 1 measured it; what is switched off
    is listed in ADR-010's progress.md, for the feature table. What is not set here takes LVGL's own default
    (include/lvgl/config/lv_conf_internal.h).
*/

// One bit a pixel, rows of bytes as LVGL draws them; the panel's columns are a conversion at the flush
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_I1

// LVGL's own heap, a static block. It holds the view's objects, a frame's draw tasks while it draws, and a glyph's
// working buffer (up to 384 bytes for an 18 px figure). A settings page of seven rows held 7,636 bytes at most (gate
// 1). The redesigned view (33 objects) left 620 bytes in one piece at rest in 12 KB, and its larger glyphs failed to
// draw; 14 KB since. `screen` says the most used and the least free in one piece
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (14 * 1024)

// One task draws, the menu's: no locking
#define LV_USE_OS LV_OS_NONE

// Software drawing into one-bit buffers, without rounded corners, shadows and masks
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 0
#define LV_DRAW_SW_SUPPORT_RGB565 0
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0
#define LV_DRAW_SW_SUPPORT_RGB565A8 0
#define LV_DRAW_SW_SUPPORT_RGB888 0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_DRAW_SW_SUPPORT_ARGB8888 0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8 0
#define LV_DRAW_SW_SUPPORT_AL88 0
#define LV_DRAW_SW_SUPPORT_A8 1
#define LV_DRAW_SW_SUPPORT_I1 1
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (2 * 1024)

// Nothing the menu does not use
#define LV_USE_OBSERVER 0
#define LV_USE_OBJ_ID_BUILTIN 0
#define LV_USE_OBJ_PROPERTY_NAME 0
#define LV_USE_CHECK_ARG 0
#define LV_USE_LOG 0

// The menu's own styles: no theme, no layout engine
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0
#define LV_USE_FLEX 0
#define LV_USE_GRID 0

// The menu's fonts, made from the BDF files in fonts/ (scripts/bdf2lvgl.py); none of LVGL's own
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(menu_font_regular) LV_FONT_DECLARE(menu_font_bold) \
                               LV_FONT_DECLARE(menu_font_figure)
#define LV_FONT_DEFAULT &menu_font_regular

// Labels only
#define LV_USE_LABEL 1
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_ARCLABEL 0
#define LV_USE_BAR 0
#define LV_USE_BUTTON 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGE 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

// The set-up's code (ADR-008, gate 4): only LVGL's copy of Nayuki's encoder is used (qrcodegen.c), the view paints
// the modules itself. LVGL builds it only with the QR code widget, the widget only with the canvas, the canvas only
// with the image (lv_conf_internal.h refuses otherwise): the widget and the canvas are left out at the link, the
// image's support stays, some 3.7 KB with the encoder's 2.4 KB (the Pico W, 2026-11-19)
#define LV_USE_QRCODE 1
#undef LV_USE_CANVAS
#define LV_USE_CANVAS 1
#undef LV_USE_IMAGE
#define LV_USE_IMAGE 1

#endif  // LV_CONF_H
