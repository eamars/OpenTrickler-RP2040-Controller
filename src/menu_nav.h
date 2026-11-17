#ifndef MENU_NAV_H_
#define MENU_NAV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

/*
    The menu's navigator (ADR-010, gate 4; menu_design.md; the skill embedded-ui-design for how a screen looks). The
    pages as one table, the stack of what is open (pages, a choice's list, the number editor, a message, a job's
    screen) with each one's focus, and the four keys. It knows no pixels and no toolkit: it fills a screen model
    (menu_screen_t) that a view draws (menu_view_12864.c, one per kind of display) and the console prints (`screen`).
    What it does is the API's own entry points, by button. Only the menu's task calls it.

    Six kinds of screen, as the design has them:
      list    a title and rows of label and value, one focused
      card    read-only lines, no focus, scrolled by the knob
      dialog  a message of one or two lines, and two softkeys
      notes   lines that end in a button, reached only by scrolling to it
      edit    a number, digit by digit, as a figure; two softkeys
      job     the title is the status; one figure and its unit; one line under it; two softkeys
    The left softkey says what Back does (the reset button, or the knob held), the right what a click does.
*/

typedef enum {
    MENU_KEY_NEXT,      // the knob a detent clockwise
    MENU_KEY_PREV,      // anticlockwise
    MENU_KEY_ENTER,     // a click
    MENU_KEY_BACK,      // the reset button, or the knob held
} menu_key_t;

#define MENU_ROWS_MAX 14
// The charge's chart: the throw's weights, at most this many, each 0 to MENU_CHART_FULL for 0 to 1.25 of the target
#define MENU_CHART_POINTS 120
// A point at each step of the throw's time, from 100 ms; across, 10 s or the throw's time when longer
#define MENU_CHART_STEP_MS 100
#define MENU_CHART_ACROSS_MS 10000
#define MENU_CHART_FULL 250
#define MENU_CHART_TARGET 200
#define MENU_TEXT 28

typedef enum {
    MENU_SCREEN_LIST,
    MENU_SCREEN_CARD,
    MENU_SCREEN_DIALOG,
    MENU_SCREEN_NOTES,
    MENU_SCREEN_EDIT,
    MENU_SCREEN_JOB,
    MENU_SCREEN_CONTROL,    // a job of rows (the cleanup): a line to read (row 0), rows set in place, softkeys
} menu_screen_kind_t;

// What is on screen, in words: what a view draws and the console prints
typedef struct {
    menu_screen_kind_t kind;
    char id[32];                            // the page's id ("settings/scale"), or "job/charge", "edit", "message"
    char title[MENU_TEXT];                  // on a job's screen, its status
    char title_note[16];                    // at the title's right, before the marks: the charge's time
    bool title_back;                        // the title is a Back target ("< Settings")

    /*
        The rows: a list's, each a label and a value; a card's or notes' lines, each a label (a value only on a
        card, after its label); a dialog's message, in its labels. The focused row on a list (-1: the title, which is
        Back); the first line shown on a card or notes
    */
    uint8_t row_count;
    char row_label[MENU_ROWS_MAX][MENU_TEXT];
    char row_value[MENU_ROWS_MAX][MENU_TEXT];
    bool row_opens[MENU_ROWS_MAX];          // opens a page, a list or the editor
    bool row_bold[MENU_ROWS_MAX];           // a heading, on notes
    bool row_chosen[MENU_ROWS_MAX];         // a choice's current option, marked
    bool row_info[MENU_ROWS_MAX];           // a row only to read: the focus passes it
    int8_t focus;
    uint8_t first;

    // Notes: the button their lines end in, and whether it is reached (shown, and a click presses it)
    char button[MENU_TEXT];
    bool button_reached;

    // The editor and a job: the figure, its unit, the line under it; the digit being set (an index into figure)
    char figure[16];
    char unit[8];
    char context[MENU_TEXT];
    bool context_bold;                      // a verdict, not a fact

    // The charge's screen with Charge view on Chart: its head ("20.08 / 30.00"), the powder or the verdict in
    // context, and the throw's weights
    bool chart;
    char chart_head[MENU_TEXT];
    uint8_t chart_count;
    uint32_t chart_step_ms;                 // point n is the weight at n steps into the throw
    uint8_t chart_points[MENU_CHART_POINTS];
    int8_t edit_at;                         // -1 on a job, and on the editor while a softkey has the focus

    // The softkeys: Back's at the left, the action at the right ("" for none). The focused one, which a click does:
    // 0 the left, 1 the right, -1 neither (the editor while a digit has the focus)
    char softkey_left[MENU_TEXT];
    char softkey_right[MENU_TEXT];
    int8_t softkey_focus;
    bool editing;                           // the editor: the digit at edit_at is being set

    // The marks on every screen (menu_design.md, "Status on every screen")
    bool mark_station;                      // joined a network
    bool mark_access_point;                 // the unit's own access point
    bool mark_remote;                       // controlled from elsewhere: the job's start, or a remote write lately
    bool mark_simulated;                    // the simulated scale stands in for the scale

    uint32_t version;                       // changes when another screen is open: a view builds it again
} menu_screen_t;

/*
    A text's width in the view's faces, so the navigator can wrap notes into the lines it scrolls through and still
    know no pixels: the view gives it at its start (menu_view_init), with the width a line may take
*/
typedef int (*menu_text_width_fn)(const char *text, bool bold);

#ifdef __cplusplus
extern "C" {
#endif

// The main menu open
void menu_nav_init(void);

// The view's measure of text, and the widest line a card or notes may hold, in its pixels
void menu_nav_set_text_width(menu_text_width_fn width, int line_width, uint8_t lines_shown);

// A key, from the knob, the buttons or the console
void menu_nav_key(menu_key_t key);

// A job began, whoever started it: its screen goes on top (an editor open is closed, nothing stored)
void menu_nav_job_begun(activity_t job);
// The job ended (jobs_take_end()): its screen goes, and a start refused for a motor driver shows the fault
void menu_nav_job_ended(uint8_t exit_form);
bool menu_nav_in_job(void);
// Each refresh: what a job's state makes stale closes (an editor over the charge once its throw begins)
void menu_nav_refresh(void);

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
