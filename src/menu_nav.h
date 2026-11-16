#ifndef MENU_NAV_H_
#define MENU_NAV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

/*
    The menu's navigator (ADR-010, gate 4; menu_design.md). The pages as one table, the stack of what is open (pages,
    a choice's list, the number editor, a message, a job's screen) with each one's focus, and the four keys. It knows
    no pixels and no toolkit: it fills a screen model (menu_screen_t) that a view draws (menu_view_12864.c, one per
    kind of display) and the console prints (`screen`). What it does is the API's own entry points, by button.
    Only the menu's task calls it.
*/

typedef enum {
    MENU_KEY_NEXT,      // the knob a detent clockwise
    MENU_KEY_PREV,      // anticlockwise
    MENU_KEY_ENTER,     // a click
    MENU_KEY_BACK,      // the reset button, or the knob held
} menu_key_t;

#define MENU_ROWS_MAX 12
#define MENU_TEXT 28

typedef enum {
    MENU_SCREEN_LIST,   // a page of rows, a choice's list, a message
    MENU_SCREEN_EDIT,   // a number, digit by digit
    MENU_SCREEN_JOB,    // the charge, a cleanup, an update
} menu_screen_kind_t;

// What is on screen, in words: what a view draws and the console prints
typedef struct {
    menu_screen_kind_t kind;
    char id[32];                            // the page's id ("settings/scale"), or "job/charge", "edit", "message"
    char title[MENU_TEXT];
    bool title_back;                        // the title is a Back target ("< Settings")

    // A list: its rows, each a label and a value; the focused row (-1: the title, which is Back)
    uint8_t row_count;
    char row_label[MENU_ROWS_MAX][MENU_TEXT];
    char row_value[MENU_ROWS_MAX][MENU_TEXT];
    bool row_opens[MENU_ROWS_MAX];          // opens a page, a list or the editor
    int8_t focus;

    // The editor: the number as its digits show it, the one being set (an index into edit_text)
    char edit_text[12];
    int8_t edit_at;

    // A job: its state, its large figure, three lines, and the action a click does now ("" for none)
    char job_state[MENU_TEXT];
    char job_big[16];
    char job_line[3][MENU_TEXT];
    char job_action[MENU_TEXT];
    bool job_actions_more;                  // the knob moves between more than one action

    // The marks on every screen (menu_design.md, "Status on every screen")
    bool mark_station;                      // joined a network
    bool mark_access_point;                 // the unit's own access point
    bool mark_remote;                       // controlled from elsewhere: the job's start, or a remote write lately
    bool mark_simulated;                    // the simulated scale stands in for the scale

    uint32_t version;                       // changes when another screen is open: a view builds it again
} menu_screen_t;

#ifdef __cplusplus
extern "C" {
#endif

// The main menu open
void menu_nav_init(void);

// A key, from the knob, the buttons or the console
void menu_nav_key(menu_key_t key);

// A job began, whoever started it: its screen goes on top (an editor open is closed, nothing stored)
void menu_nav_job_begun(activity_t job);
// The job ended (jobs_take_end()): its screen goes, and a start refused for a motor driver shows the fault
void menu_nav_job_ended(uint8_t exit_form);
bool menu_nav_in_job(void);

// The screen as it is now, live values read now
void menu_nav_screen(menu_screen_t *screen);

// The console: every page's id, and a page opened by its id (its parents under it, as the knob opens them)
size_t menu_nav_page_count(void);
const char *menu_nav_page_id(size_t idx);
bool menu_nav_goto(const char *id);

#ifdef __cplusplus
}
#endif

#endif  // MENU_NAV_H_
