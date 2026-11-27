#ifndef MENU_VIEW_H_
#define MENU_VIEW_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "menu.h"
#include "menu_nav.h"

/*
    A view, as the menu's task uses it (ADR-006, gate 6): what draws the navigator's screen (menu_screen_t) on a panel
    and keeps the frame the panel shows. The menu names no view and no panel: the start-up's wiring hands it one
    (menu_set_view()), and a build has one. Today the mini 12864's (ui/view_12864).

    frame_copy and frame_times are for any task. Every other call is the menu's task's, but prepare, which the
    start-up calls before any task runs.
*/
typedef struct {
    // The frame, for the web's mirror and the console's snap: its size, and its layout by name (MENU_FRAME_FORMAT)
    uint16_t width;
    uint16_t height;
    size_t frame_bytes;         // MENU_FRAME_BYTES_MAX at most
    const char *format;
    // The hello's steps: visit() is given 0 to visit_steps - 1
    int visit_steps;

    // Before any task runs: the frame's lock, since other tasks copy the frame
    bool (*prepare)(void);
    // The drawing library, the panel's frame and the view's objects. It gives the navigator its measure of text
    // (menu_nav_set_text_width()), so it comes before menu_nav_init()
    void (*init)(void);
    // The screen drawn, and what changed of the frame sent to the panel
    void (*draw)(const menu_screen_t *screen);
    // The whole frame sent again, so a glitch on the panel's cable does not stay on the glass
    void (*resend)(void);
    // The panel shown inverted or not. Sent only when it changes; a copy of the frame is inverted with it
    void (*set_inverted)(bool inverted);
    // The hidden hello's step shown over the page (false past the last), and the hello hidden
    bool (*visit)(int step, const char *name);
    void (*visit_end)(void);
    // The frame whole, as the panel shows it
    void (*frame_copy)(uint8_t *out);
    // How long the last frame took to draw and to send
    void (*frame_times)(menu_frame_times_t *times);
    // The drawing library's heap: the most it has held, and the most it has free in one piece now
    void (*heap)(uint32_t *max_used, uint32_t *biggest_free);
} menu_view_t;

// The largest frame a view may have: the mini 12864's, 128 by 64 at one bit
#define MENU_FRAME_BYTES_MAX 1024

#ifdef __cplusplus
extern "C" {
#endif

// The start-up's wiring, before menu_init(): the build's one view. The struct is kept by its address
void menu_set_view(const menu_view_t *view);

#ifdef __cplusplus
}
#endif

#endif  // MENU_VIEW_H_
