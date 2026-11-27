#ifndef MENU_VIEW_12864_H_
#define MENU_VIEW_12864_H_

#include "menu_view.h"

/*
    The mini 12864's view (ADR-010, gate 4): the navigator's screen (menu_screen_t) drawn with LVGL on 128 x 64 one-bit
    pixels, in the fonts the menu always had (Helvetica 8, ProFont 22 for figures). It knows the panel's size and
    its fonts, and nothing of the menu: another display is another view of the same screen.

    It owns LVGL and the screen's one frame (ADR-006, gate 6; the menu's task did before): LVGL draws only what
    changed into it, under the frame's lock, and the pages of the panel that changed are sent (display.c).
*/

#ifdef __cplusplus
extern "C" {
#endif

// The mini 12864's view, for the start-up's wiring to hand the menu (menu_set_view())
const menu_view_t *menu_view_12864(void);

#ifdef __cplusplus
}
#endif

#endif  // MENU_VIEW_12864_H_
