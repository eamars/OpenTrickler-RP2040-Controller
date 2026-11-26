#ifndef MENU_H_
#define MENU_H_

#include <stdbool.h>
#include <stdint.h>

#include "menu_nav.h"

/*
    The menu's task (ADR-010, gate 4): LVGL and the mini 12864, the knob and the buttons as four keys, the navigator
    (menu_nav.c), and the jobs' starts and ends. A job runs in the job task (jobs.c); its screen is the menu's.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The menu's locks and its screen's state, before any task runs: the web and the console may read the frame
 *        before the menu's task has started.
 */
void menu_init(void);

void menu_task(void *p);

// The screen's frame over the web and in a snap (ADR-010, gate 4): LVGL's own, DISPLAY_HEIGHT rows of
// DISPLAY_ROW_BYTES, the leftmost pixel the top bit, 1 a lit pixel as the panel shows it, its inversion applied (laid
// out as the body of a PBM image). No older format is served
#define MENU_FRAME_FORMAT "mono_rows"

/**
 * @brief A copy of the frame, whole and as the panel shows it: LVGL draws it under the same lock. Any task.
 * @param out DISPLAY_FRAME_BYTES.
 */
void menu_frame_copy(uint8_t *out);

/**
 * @brief How long the last frame took to draw (LVGL, only what changed) and to send (only the pages that changed),
 *        and the longest of each since the start, in microseconds. The whole frame is sent again once a second.
 */
typedef struct {
    uint32_t draw_us;
    uint32_t draw_max_us;
    uint32_t send_us;
    uint32_t send_max_us;
    uint32_t frames;
} menu_frame_times_t;
void menu_frame_times(menu_frame_times_t *times);

/**
 * @brief 0 while the menu's pages are on screen; -1 while a job's screen is, or before the menu starts. A start from
 *        outside is refused only while a job runs: a job takes the screen from any page (menu_design.md).
 */
int menu_get_current_form_id(void);

/**
 * @brief Wake the menu to look at a start asked from outside it (activity_request_start()), or a job's end. Never
 *        waits: with its queue full, the menu still looks within its next refresh.
 */
void menu_wake(void);

/**
 * @brief The remote control's `screen <id>` (ADR-001): open a page by its id, pressing nothing, and wait until it is
 *        drawn. Call from another task.
 * @param found set to whether the page exists and could be opened (not while a job has the screen).
 * @return false when the menu did not answer within a second.
 */
bool menu_goto_screen(const char *id, bool *found);

/**
 * @brief The screen last drawn, in words, and the most LVGL's heap has held: the remote control's `screen`. Any task.
 */
void menu_screen_copy(menu_screen_t *screen, uint32_t *lvgl_heap_max);
// The least LVGL's heap has had free in one piece since the start, sampled each second
uint32_t menu_lvgl_biggest_free(void);

/**
 * @brief The hidden hello (the owner, 2026-11-17): an agent says who it is, and the screen shows it for about 2 s,
 *        flashing first when no agent has said hello for MENU_VISIT_QUIET_MS. Any task.
 * @param name who is watching: printable ASCII, MENU_VISIT_NAME_MAX characters at most.
 * @param flash set to whether the screen flashes first.
 * @return false while a charge runs: its screen stays. Over every other page and job it is shown.
 */
#define MENU_VISIT_NAME_MAX 16
#define MENU_VISIT_QUIET_MS (10u * 60u * 1000u)
bool menu_visit(const char *name, bool *flash);


#ifdef __cplusplus
}
#endif

#endif  // MENU_H_
