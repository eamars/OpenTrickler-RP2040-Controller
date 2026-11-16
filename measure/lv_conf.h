#ifndef LV_CONF_H
#define LV_CONF_H

/*
    LVGL for ADR-010's measure build (gate 1), trimmed to what a 128 x 64 one-bit screen shows. What is not set here
    takes LVGL's own default (include/lvgl/config/lv_conf_internal.h). What is switched off is listed in ADR-010's
    progress.md, for the feature table.
*/

// One bit a pixel, rows of bytes as LVGL draws them; the panel's own column layout is a conversion at the flush
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_I1

// LVGL's own heap, a static block: its high-water mark (lv_mem_monitor) says what the screens need
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
// A settings page of seven rows held 7,636 bytes at most (the Pico 2 W, 2026-11-17)
#define LV_MEM_SIZE (12 * 1024)

// One task draws: no locking
#define LV_USE_OS LV_OS_NONE

// Software drawing, without rounded corners, shadows and masks, and into one-bit buffers only
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

// The mono theme only, one layout
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

// A one-bit pixel font in place of the anti-aliased Montserrat
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_UNSCII_8 1
#define LV_FONT_DEFAULT &lv_font_unscii_8

// The widgets a menu of rows, an editor and a progress line need
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_LIST 1
#define LV_USE_BAR 1
#define LV_USE_IMAGE 1

#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_ARCLABEL 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
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

#endif  // LV_CONF_H
