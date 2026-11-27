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

// The panel shown inverted or not: what must look the same either way (the set-up's code, dark on a light ground as a
// phone reads it) is drawn the other way round in the frame
void menu_view_set_inverted(bool inverted);

// What LVGL does not draw, painted into the frame's rows (1 lit) where it flushes an area: the charge's chart.
// Called from the flush, under the frame's lock
void menu_view_paint(uint8_t *rows, const lv_area_t *area);

// The hidden hello (the owner, 2026-11-17): Claude-chan, saying `name` is watching, over the page. Its steps are
// shown in turn, from 0; the end hides it. Its objects are made at the start, so a hello never waits on LVGL's heap.
// False past the last step
#define MENU_VIEW_VISIT_STEPS 10
bool menu_view_visit(int step, const char *name);
void menu_view_visit_end(void);

#ifdef __cplusplus
}
#endif

#endif  // MENU_VIEW_12864_H_
