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

void menu_task(void *p);

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
 * @brief The remote control's `press hold`: the knob's button down, then up after `ms` (a hold is Back).
 */
bool menu_hold(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif  // MENU_H_
