#ifndef MENU_VIEW_12864_H_
#define MENU_VIEW_12864_H_

#include "lvgl/lvgl.h"

#include "menu_nav.h"

/*
    The mini 12864's view (ADR-010, gate 4): the navigator's screen (menu_screen_t) drawn with LVGL on 128 x 64 one-bit
    pixels, in the fonts the menu always had (Helvetica 8, ProFont 22 for figures). It knows the panel's size and
    its fonts, and nothing of the menu: another display is another view of the same screen. The menu's task only.
*/

#ifdef __cplusplus
extern "C" {
#endif

// Its objects, on the display's screen
void menu_view_init(lv_display_t *display);

// The screen drawn: what changed is set, LVGL draws it at its next timer
void menu_view_draw(const menu_screen_t *screen);

#ifdef __cplusplus
}
#endif

#endif  // MENU_VIEW_12864_H_
