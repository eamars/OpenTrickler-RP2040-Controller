/*
    The menu's navigator (ADR-010, gate 4): the tree of menu_design.md as one table, what is open, and the keys.
    Every write goes through the entry point the API uses for the same thing, by button; a setting is stored when it
    is confirmed (D2). No row draws: menu_nav_screen() says what is on screen, in words, in the kinds of screen the
    skill embedded-ui-design sets (lists, dialogs, notes, the editor, jobs, the cleanup's control).
*/
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "access_point_mode.h"
#include "activity.h"
#include "api_v1.h"
#include "charge_mode.h"
#include "calibration.h"
#include "cleanup_mode.h"
#include "common.h"
#include "eeprom.h"
#include "unit_id.h"
#include "events.h"
#include "http_server.h"
#include "jobs.h"
#include "menu_nav.h"
#include "motors.h"
#include "profile.h"
#include "scale.h"
#include "servo_gate.h"
#include "settings.h"
#include "setup.h"
#include "system_control.h"
#include "update.h"
#include "update_poll.h"
#include "version.h"
#include "wireless.h"

// The calibration's records, where they are kept (ADR-009 step 1): read by its page and its offer
extern charge_mode_config_t charge_mode_config;
extern scale_config_t scale_config;

// A write to the API this recent counts as the unit being controlled from elsewhere (menu_design.md, proposed)
#define MENU_REMOTE_MS 30000

#define MENU_FRAMES_MAX 8
#define MENU_EDIT_DIGITS 5

typedef struct menu_row menu_row_t;
typedef struct menu_page menu_page_t;

typedef enum {
    ROW_PAGE,       // opens a page
    ROW_ACTION,     // does something
    ROW_CHOICE,     // one of a list, chosen on its own page, the current one checked
    ROW_NUMBER,     // a number, which the editor sets: the target
    ROW_STEP,       // a setting's number, stepped in its row by its field's edit_step, live, stored at the click
    ROW_TOGGLE,     // on or off, flipped by a click
    ROW_INFO,       // read only
} row_kind_t;

struct menu_row {
    const char *label;                                                      // NULL: the setting's, from its table
    row_kind_t kind;
    const menu_page_t *page;                                                // ROW_PAGE
    void (*value)(const menu_row_t *row, char *out, size_t room);           // the value at the right; NULL: none
    const char *(*label_now)(void);                                         // a label that changes; NULL: label
    bool (*shown)(void);                                                    // NULL: always
    void (*act)(const menu_row_t *row);                                     // ROW_ACTION, ROW_TOGGLE
    const char *path;                                                       // a setting: its resource
    const char *name;                                                       // and its field
    bool (*offered)(const setting_choice_t *choice);                        // a setting's choice offered; NULL: all
    void (*stepped)(const menu_row_t *row);                                 // ROW_STEP: after each live write
    // A choice that is not a setting's (the powder profiles)
    int (*count)(void);
    const char *(*option)(int idx);
    int (*current)(void);
    const char *(*choose)(int idx);                                         // NULL when done, else why not
    bool bold;                                                              // a heading, on notes
};

// What kind of screen a page is drawn as (the skill embedded-ui-design, rule 5)
typedef enum {
    PAGE_LIST = 0,      // rows, one focused
    PAGE_DIALOG,        // its info rows the message, its action the right softkey, Back the left (Cancel)
    PAGE_NOTES,         // lines, wrapped, that end in its action row: the button, reached only by scrolling
    PAGE_CODE,          // the set-up (ADR-008): its stage as the unit keeps it; at its start the code, its info rows
                        // the lines beside it
} page_kind_t;

struct menu_page {
    page_kind_t kind;
    const char *id;
    const char *title;
    const char *(*title_now)(void);                                         // a title that changes; NULL: title
    const menu_row_t *rows;
    uint8_t row_count;
    bool cancel_first;      // a dialog that destroys: it opens with Cancel focused
};

typedef enum {
    FRAME_PAGE,
    FRAME_CHOICE,
    FRAME_EDIT,
    FRAME_MESSAGE,
    FRAME_JOB,
} frame_kind_t;

typedef struct {
    frame_kind_t kind;
    const menu_page_t *page;            // FRAME_PAGE
    const menu_row_t *row;              // FRAME_CHOICE, FRAME_EDIT: the row it sets
    int8_t focus;                       // a shown row's index; -1 the title, which is Back
    uint8_t digits[MENU_EDIT_DIGITS];   // FRAME_EDIT: most significant first
    uint8_t decimals;
    int8_t digit_at;
    char message[3][MENU_TEXT];         // FRAME_MESSAGE: a title and two lines
    activity_t job;                     // FRAME_JOB
    uint8_t first;                      // FRAME_PAGE, a card or notes: the first line shown
    bool at_end;                        // FRAME_PAGE, notes scrolled to their end: they stay there when lines come,
                                        // so their button stays reached (Wi-Fi's "Takes effect at restart")
    // The softkey focused on a dialog, a message or a job: 0 the left, 1 the right. The knob moves between them and
    // a click does the focused one (the owner: every element on screen is reached by the knob)
    int8_t softkey;
    bool editing;                       // FRAME_EDIT: the focused digit is being set; digit_at past the digits is a
                                        // softkey (MENU_EDIT_DIGITS the left, the next the right). FRAME_PAGE: the
                                        // focused row's number is being set in place
    char edit_was[16];                  // FRAME_PAGE, a number being set: its value before, for Back
    uint8_t action_seen;                // FRAME_JOB, the charge: its action when last drawn, so a new one is focused
    uint32_t code_seen;                 // PAGE_CODE: the hotspot's addresses given when the code was asked for
    bool code_shown;                    // PAGE_CODE: "Show the code" asked, until a phone joins or Back
    uint8_t stage;                      // PAGE_CODE: the stage last drawn (setup_stage_t), to draw it afresh on a change
} frame_t;

static frame_t frames[MENU_FRAMES_MAX];
static int frame_depth = 0;
static uint32_t screen_version = 1;

// The view's measure of text (menu_nav_set_text_width), to wrap cards and notes into the lines they scroll through
static menu_text_width_fn nav_text_width = NULL;
static int nav_line_width = 120;
static uint8_t nav_lines_shown = 4;


// --- Pushing and popping what is open

static frame_t *top(void) {
    return &frames[frame_depth - 1];
}


static frame_t *push(frame_kind_t kind) {
    if (frame_depth == MENU_FRAMES_MAX) {
        frame_depth -= 1;     // the deepest is dropped rather than refuse a page: never seen with the tree's depth
    }
    frame_t *frame = &frames[frame_depth++];
    memset(frame, 0, sizeof(*frame));
    frame->kind = kind;
    screen_version += 1;
    return frame;
}


static void pop(void) {
    if (frame_depth > 1) {
        frame_depth -= 1;
        screen_version += 1;
    }
}


static bool row_shown(const menu_row_t *row) {
    return row->shown == NULL || row->shown();
}


// The page's rows shown now, as pointers; their count
static uint8_t shown_rows(const menu_page_t *page, const menu_row_t **out) {
    uint8_t count = 0;
    for (uint8_t idx = 0; idx < page->row_count && count < MENU_ROWS_MAX; idx += 1) {
        if (row_shown(&page->rows[idx])) {
            out[count++] = &page->rows[idx];
        }
    }
    return count;
}


static void push_page(const menu_page_t *page) {
    frame_t *frame = push(FRAME_PAGE);
    frame->page = page;
    // The first row that does something, so an acknowledgement's action is focused
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t count = shown_rows(page, rows);
    frame->focus = 0;
    for (uint8_t idx = 0; idx < count; idx += 1) {
        if (rows[idx]->kind != ROW_INFO) {
            frame->focus = (int8_t) idx;
            break;
        }
    }
    // A dialog opens on its action, so one click goes on; one that destroys opens on Cancel. The set-up's softkeys
    // follow its stage (menu_nav_refresh)
    frame->softkey = page->cancel_first ? 0 : 1;
    frame->code_seen = access_point_mode_addresses_given();
    frame->stage = UINT8_MAX;
}


static void push_message(const char *title, const char *line1, const char *line2) {
    frame_t *frame = push(FRAME_MESSAGE);
    frame->softkey = 1;     // its one softkey, OK
    snprintf(frame->message[0], MENU_TEXT, "%s", title);
    snprintf(frame->message[1], MENU_TEXT, "%s", line1 != NULL ? line1 : "");
    snprintf(frame->message[2], MENU_TEXT, "%s", line2 != NULL ? line2 : "");
}


// --- A setting's choices and toggles: read and written through settings.c, the API's own checks

// A choice as the screen writes it: its short label where it has one
static const char *choice_text(const setting_choice_t *choice) {
    return choice->short_label != NULL ? choice->short_label : choice->label;
}


static int setting_choice_now(const menu_row_t *row) {
    const setting_field_t *field = settings_field(row->path, row->name);
    char text[24];
    if (field == NULL || !settings_read_text(row->path, row->name, text, sizeof(text))) {
        return -1;
    }
    for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
        char choice[24];
        const setting_choice_t *option = &field->choices[idx];
        if (option->text != NULL) {
            snprintf(choice, sizeof(choice), "%s", option->text);
        }
        else {
            snprintf(choice, sizeof(choice), "%ld", (long) option->number);
        }
        if (strcmp(choice, text) == 0) {
            return idx;
        }
    }
    return -1;
}


static int row_choice_count(const menu_row_t *row) {
    if (row->count != NULL) {
        return row->count();
    }
    const setting_field_t *field = settings_field(row->path, row->name);
    return field == NULL ? 0 : field->choice_count;
}


// An option's label, or NULL when it is not offered
static const char *row_choice_label(const menu_row_t *row, int idx) {
    if (row->option != NULL) {
        return row->option(idx);
    }
    const setting_field_t *field = settings_field(row->path, row->name);
    if (field == NULL || idx < 0 || idx >= field->choice_count) {
        return NULL;
    }
    if (row->offered != NULL && !row->offered(&field->choices[idx])) {
        return NULL;
    }
    return choice_text(&field->choices[idx]);
}


static int row_choice_now(const menu_row_t *row) {
    return row->current != NULL ? row->current() : setting_choice_now(row);
}


static const char *row_choose(const menu_row_t *row, int idx) {
    if (row->choose != NULL) {
        return row->choose(idx);
    }
    const setting_field_t *field = settings_field(row->path, row->name);
    if (field == NULL || idx < 0 || idx >= field->choice_count) {
        return "no such choice";
    }
    const setting_choice_t *option = &field->choices[idx];
    char value[24];
    if (option->text != NULL) {
        snprintf(value, sizeof(value), "%s", option->text);
    }
    else {
        snprintf(value, sizeof(value), "%ld", (long) option->number);
    }
    return settings_write_and_save(row->path, row->name, value);
}


static bool setting_is_on(const menu_row_t *row) {
    char text[8];
    return settings_read_text(row->path, row->name, text, sizeof(text)) && strcmp(text, "true") == 0;
}


// A row's label: its own, or its setting's from the table (the short one where it has one)
static const char *row_label(const menu_row_t *row) {
    if (row->label_now != NULL) {
        return row->label_now();
    }
    if (row->label != NULL) {
        return row->label;
    }
    const setting_field_t *field = settings_field(row->path, row->name);
    if (field == NULL) {
        return "?";
    }
    return field->short_label != NULL ? field->short_label : field->label;
}


static void act_toggle(const menu_row_t *row) {
    const char *why = settings_write_and_save(row->path, row->name, setting_is_on(row) ? "false" : "true");
    if (why != NULL) {
        push_message("Not changed", why, NULL);
    }
}


// --- Values shown at the right of a row

static decimal_places_t charge_decimals(void) {
    char text[4];
    if (settings_read_text("settings/charge", "decimal_places", text, sizeof(text)) && strcmp(text, "3") == 0) {
        return DP_3;
    }
    return DP_2;
}


static void format_weight(float weight, char *out, size_t room) {
    if (!isfinite(weight)) {
        snprintf(out, room, "no reading");
        return;
    }
    weight_to_string(out, room, weight, charge_decimals());
}


static void value_target(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    float target = charge_mode_get_target();
    if (target <= 0.0f) {
        snprintf(out, room, "not set");
        return;
    }
    format_weight(target, out, room);
}


static void value_reading(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    format_weight(scale_get_current_measurement(), out, room);
}


static void value_version(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    update_status_t update;
    update_status(&update);
    snprintf(out, room, "%s%s", version_string, update.on_trial ? " trial" : "");
}


static bool shown_last_update(void) {
    update_status_t update;
    update_status(&update);
    return update.last.result != UPDATE_RESULT_NONE;
}


static void value_last_update(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    update_status_t update;
    update_status(&update);
    static const char *const results[] = {"none", "confirmed", "went back", "refused", "stopped", "timed out"};
    int result = (int) update.last.result;
    snprintf(out, room, "%s", result >= 0 && result < (int) (sizeof(results) / sizeof(results[0])) ?
             results[result] : "?");
}


// --- The update at the screen (ADR-010, gate 5; menu_design.md, "The update at the screen")

static bool update_found(update_poll_status_t *poll) {
    update_poll_status(poll);
    return poll->state == UPDATE_POLL_FOUND;
}


static bool shown_update_found(void) {
    update_poll_status_t poll;
    return update_found(&poll);
}


// "Update 9.0.1", or "Important update" for a critical release: no pop-up, a row (the owner)
static const char *update_row_label(void) {
    static char label[MENU_TEXT];
    update_poll_status_t poll;
    if (update_found(&poll) && poll.critical) {
        return "Important update";
    }
    snprintf(label, sizeof(label), "Update %.15s", poll.version);
    return label;
}


static bool shown_on_station(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return strcmp(info.mode, "station") == 0 && info.address[0] != '\0';
}


static bool shown_not_on_station(void) {
    return !shown_on_station();
}


static void value_update_needs_network(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "needs a network");
}


// The offer's heading: the version, and whether it matters
static const char *offer_heading(void) {
    static char line[MENU_TEXT];
    update_poll_status_t poll;
    if (!update_found(&poll)) {
        return "No update is found";        // the page opened by its id (the console), with nothing to offer
    }
    snprintf(line, sizeof(line), "%.15s%s", poll.version, poll.critical ? ", important" : " is ready");
    return line;
}


// The note as the release gives it, wrapped where it is shown (its start: the whole is on the web)
static const char *offer_note(void) {
    static char note[UPDATE_NOTE_MAX];
    update_poll_status_t poll;
    // Only a found release's: the status keeps the last one's note after a poll that failed
    if (!update_found(&poll)) {
        return "";
    }
    snprintf(note, sizeof(note), "%s", poll.note);
    return note;
}


// Found on the bench host: the offer says so, with its address, so nobody takes it for GitHub's (ADR-010, gate 6,
// step 9: nothing signs an image, so anything that announces the name could offer one)
static bool shown_update_from_bench(void) {
    update_poll_status_t poll;
    return update_found(&poll) && poll.bench[0] != '\0';
}


static const char *offer_from_bench(void) {
    static char line[48];
    update_poll_status_t poll;
    update_found(&poll);
    snprintf(line, sizeof(line), "From the bench host %s", poll.bench);
    return line;
}


// The last try of this release went back: the offer says so (menu_design.md)
static bool shown_update_went_back(void) {
    update_poll_status_t poll;
    update_status_t update;
    update_status(&update);
    return update_found(&poll) && update.last.result == UPDATE_RESULT_BACK && update.last.version == poll.block;
}


static const char *update_went_back_label(void) {
    static char line[48];
    update_poll_status_t poll;
    update_found(&poll);
    // Why, as the unit knows it (ADR-008, gate 2): not "did not start" when only the router was away
    update_status_t update;
    update_status(&update);
    const char *why = update.last.reason == EVENT_REASON_NO_NETWORK ? "found no network last time." :
                      update.last.reason == EVENT_REASON_NOT_CONFIRMED ? "was not kept last time." :
                      update.last.reason == EVENT_REASON_POWER_CUT ? "lost power last time." :
                      "did not start last time.";
    snprintf(line, sizeof(line), "%.15s %s", poll.version, why);
    return line;
}


// The last try failed before its restart: what failed is on the web (GET /api/v1/update, last)
static bool shown_update_failed(void) {
    update_status_t update;
    update_status(&update);
    return shown_update_found() &&
           (update.last.result == UPDATE_RESULT_REFUSED || update.last.result == UPDATE_RESULT_TIME);
}


static void act_update_install(const menu_row_t *row) {
    (void) row;
    switch (update_request_download(ACTIVITY_BY_BUTTON)) {
        case UPDATE_START_OK:
            // The menu's task takes it, and the update's screen comes
            break;
        case UPDATE_START_ON_TRIAL:
            push_message("Update", "This version is on trial:", "try again in a minute");
            break;
        case UPDATE_START_NOTHING_FOUND:
            push_message("Update", "Nothing to install:", "check for update again");
            break;
        case UPDATE_START_NO_SLOTS:
            push_message("Update", "No update can be", "written on this unit");
            break;
        case UPDATE_START_BUSY:
        default:
            push_message("Update", "The unit is busy:", "try again");
            break;
    }
}


static const menu_page_t page_update;

// The Firmware page's Update line: what the poll knows
static void value_update_state(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    update_poll_status_t poll;
    update_poll_status(&poll);
    static const char *const states[] = {"not checked", "checking...", "up to date", "", "failed"};
    if (poll.state == UPDATE_POLL_FOUND) {
        snprintf(out, room, "%.15s found", poll.version);
    }
    else {
        snprintf(out, room, "%s", states[poll.state]);
    }
}


// Its button: a poll, or the offer once one is found
static const char *check_update_label(void) {
    static char label[MENU_TEXT];
    update_poll_status_t poll;
    if (update_found(&poll)) {
        snprintf(label, sizeof(label), "Update to %.15s", poll.version);
    }
    else {
        snprintf(label, sizeof(label), "Check for update");
    }
    return label;
}


// Check for update: a poll, or the offer once one is found
static void act_check_update(const menu_row_t *row) {
    (void) row;
    if (shown_update_found()) {
        push_page(&page_update);
        return;
    }
    update_poll_ask_t asked = update_poll_ask();
    if (asked == UPDATE_POLL_NO_NETWORK) {
        push_message("Update", "Join a network first:", "no internet here");
    }
}


static void value_board(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", api_v1_board_name());
}


static void value_unit_id(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    char id[UNIT_ID_LENGTH + 1];
    unit_id_get(id, sizeof(id));
    snprintf(out, room, "%s", id);
}


static void value_build(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s %.7s", build_type, vcs_hash);
}


// --- The Wi-Fi page: what GET /api/v1/network gives

static bool wifi_is_station(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return strcmp(info.mode, "station") == 0;
}


// The station's network, named when it is not joined (the first line says why)
static bool wifi_has_trouble(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return info.trouble != NULL;
}


static void value_wifi_station_network(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    char ssid[40];
    snprintf(out, room, "%s", settings_read_text("settings/wifi", "ssid", ssid, sizeof(ssid)) ? ssid : "?");
}


// Wi-Fi off or on, at its next start: the button at the end of its page, which only turns it, so a press is undone by
// another; a line above it says when a change waits for a restart
static bool wifi_enabled(void) {
    char text[8];
    return settings_read_text("settings/wifi", "enable", text, sizeof(text)) && strcmp(text, "true") == 0;
}


static const char *wifi_button_label(void) {
    return wifi_enabled() ? "Turn Wi-Fi off" : "Turn Wi-Fi on";
}


static void act_wifi_button(const menu_row_t *row) {
    (void) row;
    const char *why = settings_write_and_save("settings/wifi", "enable", wifi_enabled() ? "false" : "true");
    if (why != NULL) {
        push_message("Not changed", why, NULL);
    }
}


static bool wifi_is_access_point(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return strcmp(info.mode, "access_point") == 0;
}


// The set-up's offer on the main menu (ADR-008, gate 4): a unit never set up, on its own hotspot; gone once set up
static bool setup_offered(void) {
    return wifi_is_access_point() && !wireless_set_up();
}


/*
 * The Wi-Fi page's button: on its own hotspot with no network named, the code that joins the hotspot; where a network
 * is named, its switch, the hotspot standing in only while that network is away (gate 1's mock)
 */
static bool wifi_code_offered(void) {
    char ssid[40];
    return wifi_is_access_point() && (!settings_read_text("settings/wifi", "ssid", ssid, sizeof(ssid)) ||
                                      ssid[0] == '\0');
}


static bool wifi_switch_offered(void) {
    return !wifi_code_offered();
}


/*
 * The code the phones' cameras read to join a Wi-Fi network: WIFI:T:WPA;S:<name>;P:<key>;; The format escapes
 * \ ; , : and " with a backslash; the hotspot's name (opentrickler- and hex digits) and its key (unit_id.c's
 * alphabet) have none of them
 */
static void setup_code_text(char *out, size_t room) {
    wireless_info_t info;
    wireless_get_link(&info);
    snprintf(out, room, "WIFI:T:WPA;S:%s;P:%s;;", info.hostname != NULL ? info.hostname : "",
             access_point_mode_key());
}


// The set-up's stage on the 12864 (README, "The set-up, designed again": the table's 12864 column), as the unit knows
// it, so it is true whether or not the phone still reaches the unit
typedef enum {
    STAGE_CODE = 0,     // the code: no phone joined yet, or "Show the code" asked
    STAGE_CHOOSE,       // a phone joined; it chooses a network
    STAGE_PASSWORD,     // a network chosen; its password typed on the phone
    STAGE_JOINING,
    STAGE_JOINED,
    STAGE_WRONG_PASSWORD,
    STAGE_NO_ANSWER,
    STAGE_NOT_JOINED,
    STAGE_SCALE,
    STAGE_MOTORS,
    STAGE_READY,
    STAGE_DONE,
} setup_stage_t;


static setup_stage_t setup_stage(const frame_t *frame) {
    if (frame->code_shown) {
        return STAGE_CODE;
    }
    switch (setup_step()) {
        case SETUP_STEP_DONE:
            return STAGE_DONE;
        case SETUP_STEP_READY:
            return STAGE_READY;
        case SETUP_STEP_MOTORS:
            return STAGE_MOTORS;
        case SETUP_STEP_SCALE:
            return STAGE_SCALE;
        case SETUP_STEP_CODE:
            return wifi_is_access_point() && access_point_mode_addresses_given() > 0 ? STAGE_CHOOSE : STAGE_CODE;
        case SETUP_STEP_WIFI:
        default:
            break;
    }
    wireless_station_info_t station;
    wireless_get_station(&station);
    char network[SETUP_NETWORK_MAX];
    setup_network(network, sizeof(network));
    if (strcmp(station.state, "joining") == 0) {
        return STAGE_JOINING;
    }
    bool this_network = network[0] == '\0' || strcmp(station.ssid, network) == 0;
    if (strcmp(station.state, "joined") == 0 && this_network) {
        return STAGE_JOINED;
    }
    if (strcmp(station.state, "failed") == 0 && this_network && network[0] != '\0') {
        if (station.reason != NULL && strcmp(station.reason, "wrong_password") == 0) {
            return STAGE_WRONG_PASSWORD;
        }
        return station.reason != NULL && strcmp(station.reason, "timeout") == 0 ? STAGE_NO_ANSWER : STAGE_NOT_JOINED;
    }
    return network[0] != '\0' ? STAGE_PASSWORD : STAGE_CHOOSE;
}


// The link as a key, and as a station the network it joins as its value
static const char *wifi_mode_label(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    if (info.trouble != NULL) {
        return "Not joined";
    }
    if (strcmp(info.mode, "access_point") == 0) {
        return "Its own access point";
    }
    if (strcmp(info.mode, "station") == 0) {
        return info.address[0] != '\0' ? "Joined" : "Joining";
    }
    return "Not joined";
}


static void value_wifi_mode(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    if (info.trouble != NULL) {
        snprintf(out, room, "%s", info.trouble);
        return;
    }
    snprintf(out, room, "%s", strcmp(info.mode, "station") == 0 ? info.ssid : "");
}


static void value_wifi_network(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    snprintf(out, room, "%s", info.ssid);
}


static void value_wifi_key(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", access_point_mode_key());
}


static void value_wifi_address(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    snprintf(out, room, "%s", info.address[0] != '\0' ? info.address : "none");
}


// The name the unit answers to on the network: what a browser is given
static void value_wifi_name(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    snprintf(out, room, "%s.local", info.hostname != NULL ? info.hostname : "");
}


static void value_wifi_signal(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_info(&info);
    if (info.has_rssi) {
        snprintf(out, room, "%ld dBm", (long) info.rssi);
    }
    else {
        out[0] = '\0';
    }
}


// --- Actions

static const char *restart_label(void) {
    return settings_restart_pending() ? "Restart to apply" : "Restart";
}


static bool shown_restart_pending(void) {
    return settings_restart_pending();
}


static void act_restart(const menu_row_t *row) {
    (void) row;
    software_reboot();
}


static void act_erase(const menu_row_t *row) {
    (void) row;
    eeprom_erase(true);
}


static void act_zero(const menu_row_t *row) {
    (void) row;
    scale_ask_zero();
}


static bool motors_fault_shown(const char *what) {
    if (motors_ready()) {
        return false;
    }
    push_message(motors_get_fault(), what, "cannot start");
    return true;
}


// A page's rows counted from its list, never by hand: a row left out of a build is out of the count too
#define ROW_COUNT(list) ((uint8_t) (sizeof(list) / sizeof((list)[0])))

static const menu_page_t page_ack_charge;
static const menu_page_t page_ack_cleanup;
static const menu_row_t rows_charge[3];
static void push_edit(const menu_row_t *row);

// The charge page's Target and Powder, which the charge's own screen opens too between throws
#define ROW_TARGET (&rows_charge[1])


static void act_charge_start(const menu_row_t *row) {
    (void) row;
    // With no target, its editor first
    if (charge_mode_get_target() <= 0.0f) {
        push_edit(ROW_TARGET);
        return;
    }
    if (!motors_fault_shown("Charge")) {
        push_page(&page_ack_charge);
    }
}


static void act_cleanup_start(const menu_row_t *row) {
    (void) row;
    if (!motors_fault_shown("Purge")) {
        push_page(&page_ack_cleanup);
    }
}


// The cup acknowledged (D1): the start, as the API's. The menu's task takes it and the job's screen comes
static void act_ack_charge(const menu_row_t *row) {
    (void) row;
    charge_start_result_t result = charge_mode_request_start(ACTIVITY_BY_BUTTON, NAN, -1);
    if (result == CHARGE_START_DRIVER_FAULT) {
        push_message(motors_get_fault(), "Charge", "cannot start");
    }
    else if (result == CHARGE_START_NO_TARGET) {
        push_message("No target", "Set a target first", NULL);
    }
    else if (result != CHARGE_START_OK) {
        push_message("Busy", "A job runs", NULL);
    }
}


static void act_ack_cleanup(const menu_row_t *row) {
    (void) row;
    cleanup_start_result_t result = cleanup_mode_request_start(ACTIVITY_BY_BUTTON);
    if (result == CLEANUP_START_DRIVER_FAULT) {
        push_message(motors_get_fault(), "Purge", "cannot start");
    }
    else if (result != CLEANUP_START_OK) {
        push_message("Busy", "A job runs", NULL);
    }
}


// --- The powder profiles: chosen as PATCH /api/v1/charge chooses one, and stored with the profiles (D2)

static int powder_count(void) {
    return MAX_PROFILE_CNT;
}


static const char *powder_option(int idx) {
    return idx >= 0 && idx < MAX_PROFILE_CNT ? profile_get_name((uint16_t) idx) : NULL;
}


static int powder_current(void) {
    return (int) profile_get_selected_idx();
}


static const char *powder_choose(int idx) {
    activity_lock();
    bool in_throw = activity_in_throw();
    if (!in_throw) {
        profile_select((uint8_t) idx);
    }
    activity_unlock();
    if (in_throw) {
        return "not while a throw pours";
    }
    return settings_store("/api/v1/powder_profiles/0");    // the eight are stored together, the choice with them
}


static void value_powder(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", profile_get_name(profile_get_selected_idx()));
}


// --- The choices offered at the screen

// Every make but the simulated scale, a debug feature (the owner, D7)
static bool offered_make(const setting_choice_t *choice) {
    return choice->text == NULL || strcmp(choice->text, "simulated") != 0;
}


static void value_setting_choice(const menu_row_t *row, char *out, size_t room) {
    int idx = setting_choice_now(row);
    const setting_field_t *field = settings_field(row->path, row->name);
    snprintf(out, room, "%s", field != NULL && idx >= 0 ? choice_text(&field->choices[idx]) : "?");
}


static void value_setting_toggle(const menu_row_t *row, char *out, size_t room) {
    const setting_field_t *field = settings_field(row->path, row->name);
    bool on = setting_is_on(row);
    const char *word = field != NULL ? (on ? field->on_label : field->off_label) : NULL;
    snprintf(out, room, "%s", word != NULL ? word : (on ? "On" : "Off"));
}


// A stepped number's decimals: as many as its step has
static int step_decimals(float step) {
    return step >= 0.999f ? 0 : (step >= 0.0999f ? 1 : (step >= 0.00999f ? 2 : 3));
}


static const char *unit_text(void);

static void value_setting_step(const menu_row_t *row, char *out, size_t room) {
    const setting_field_t *field = settings_field(row->path, row->name);
    char text[16];
    if (field == NULL || !settings_read_text(row->path, row->name, text, sizeof(text))) {
        snprintf(out, room, "?");
        return;
    }
    // A weight's unit is the scale's, as the charge shows it
    bool weight = field->unit != NULL && strcmp(field->unit, "weight") == 0;
    snprintf(out, room, "%.*f%s%s", step_decimals(field->edit_step), (double) strtof(text, NULL),
             weight && unit_text()[0] != '\0' ? " " : "", weight ? unit_text() : "");
}


// --- The backlight: 8 named colours (ADR-010, gate 6: a colour as three numbers costs too much at the knob)

typedef struct {
    const char *name;
    const char *hex;
} named_colour_t;

static const named_colour_t backlight_colours[] = {
    {"White", "#ffffff"}, {"Red", "#ff0000"}, {"Orange", "#ff8000"}, {"Yellow", "#ffff00"},
    {"Green", "#00ff00"}, {"Cyan", "#00ffff"}, {"Blue", "#0000ff"}, {"Magenta", "#ff00ff"},
};
#define BACKLIGHT_COLOURS ((int) (sizeof(backlight_colours) / sizeof(backlight_colours[0])))


static int backlight_count(void) {
    return BACKLIGHT_COLOURS;
}


static const char *backlight_option(int idx) {
    return idx >= 0 && idx < BACKLIGHT_COLOURS ? backlight_colours[idx].name : NULL;
}


// The named colour the backlight is, or -1 for one set on the web
static int backlight_current(void) {
    char text[12];
    if (!settings_read_text("settings/leds", "backlight_colour", text, sizeof(text))) {
        return -1;
    }
    for (int idx = 0; idx < BACKLIGHT_COLOURS; idx += 1) {
        if (strcasecmp(text, backlight_colours[idx].hex) == 0) {
            return idx;
        }
    }
    return -1;
}


static const char *backlight_choose(int idx) {
    if (idx < 0 || idx >= BACKLIGHT_COLOURS) {
        return "no such colour";
    }
    return settings_write_and_save("settings/leds", "backlight_colour", backlight_colours[idx].hex);
}


static void value_backlight(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    int idx = backlight_current();
    snprintf(out, room, "%s", idx >= 0 ? backlight_colours[idx].name : "Custom");
}


// --- The charge's chart: the throw's weight while it pours, as a share of the target, a point at each step of the
// throw's own time (the time its title shows). When the points are full, every other one goes and the step doubles,
// so each point keeps its time and a throw of any length fits; the view places them by time (the owner, 2026-11-18)

static uint8_t chart_points[MENU_CHART_POINTS];
static uint8_t chart_count = 0;
static uint16_t chart_throw = 0;
static uint32_t chart_step_ms = MENU_CHART_STEP_MS;


static void chart_sample(void) {
    charge_status_t charge;
    charge_mode_get_status(&charge);
    if (strcmp(charge.state, "charging") != 0 || charge.target <= 0.0f) {
        return;
    }
    if (charge.throw_number != chart_throw) {
        chart_throw = charge.throw_number;
        chart_count = 0;
        chart_step_ms = MENU_CHART_STEP_MS;
    }
    float share = scale_get_current_measurement() / charge.target;
    share = isfinite(share) ? share : 0.0f;
    share = share < 0.0f ? 0.0f : (share > 1.25f ? 1.25f : share);
    uint8_t point = (uint8_t) lroundf(share * MENU_CHART_TARGET);
    // Every step's time passed since the last point takes the weight now: a refresh comes about once a step
    uint32_t now_ms = (uint32_t) lroundf(charge.elapsed_s * 1000.0f);
    while ((uint32_t) chart_count * chart_step_ms <= now_ms) {
        if (chart_count == MENU_CHART_POINTS) {
            for (uint8_t idx = 0; idx < MENU_CHART_POINTS / 2; idx += 1) {
                chart_points[idx] = chart_points[2 * idx];
            }
            chart_count = MENU_CHART_POINTS / 2;
            chart_step_ms *= 2;
            continue;
        }
        chart_points[chart_count++] = point;
    }
}


// The cleanup's flow: the weight's change over a second or more (the old screen's, over 20 ms, jumped about)
static float flow_weight = NAN;
static uint32_t flow_ms = 0;
static float flow_per_s = NAN;


static void flow_sample(void) {
    float weight = scale_get_current_measurement();
    uint32_t now_ms = (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (!isfinite(weight)) {
        flow_weight = NAN;
        flow_per_s = NAN;
        return;
    }
    if (!isfinite(flow_weight)) {
        flow_weight = weight;
        flow_ms = now_ms;
        return;
    }
    if (now_ms - flow_ms >= 1000u) {
        flow_per_s = (weight - flow_weight) * 1000.0f / (float) (now_ms - flow_ms);
        flow_weight = weight;
        flow_ms = now_ms;
    }
}


static bool chart_chosen(void) {
    char view[12];
    return settings_read_text("settings/display", "charge_view", view, sizeof(view)) && strcmp(view, "chart") == 0;
}


// --- The servo gate: moved as the API moves it (PATCH /api/v1/gate), and so refused when it is

static const char *gate_move(gate_ratio_t ratio) {
    if (!FEATURE_SERVO_GATE) {
        return "no servo gate in this build";
    }
    if (!servo_gate_enabled()) {
        return "the gate is off";
    }
    activity_lock();
    bool in_throw = activity_in_throw();
    if (!in_throw) {
        servo_gate_set_ratio(ratio, false);
    }
    activity_unlock();
    return in_throw ? "not while a throw pours" : NULL;
}


// The gate's page, and what only it uses: in a build that carries the gate
#if FEATURE_SERVO_GATE
static void value_gate(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", servo_gate_state() == GATE_OPEN ? "Open" :
                              (servo_gate_state() == GATE_CLOSE ? "Closed" : "Off"));
}


static void act_gate(const menu_row_t *row) {
    (void) row;
    const char *why = gate_move(servo_gate_state() == GATE_OPEN ? SERVO_GATE_RATIO_CLOSED : SERVO_GATE_RATIO_OPEN);
    if (why != NULL) {
        push_message("Not moved", why, NULL);
    }
}


// A shutter's end being set: the gate goes to it, so the end is seen as it is set
static void gate_show_end(const menu_row_t *row) {
    gate_move(strstr(row->name, "close") != NULL ? SERVO_GATE_RATIO_CLOSED : SERVO_GATE_RATIO_OPEN);
}
#endif


// --- The tree (menu_design.md)

// The start's acknowledgement, a dialog: a safety step, never cut (the owner)
static const menu_row_t rows_ack_charge[] = {
    {.label = "Put the empty cup", .kind = ROW_INFO},
    {.label = "on the scale", .kind = ROW_INFO},
    {.label = "Start", .kind = ROW_ACTION, .act = act_ack_charge},
};
static const menu_page_t page_ack_charge = {.kind = PAGE_DIALOG, .id = "ack/charge", .title = "Charge",
                                            .rows = rows_ack_charge, .row_count = ROW_COUNT(rows_ack_charge)};

static const menu_row_t rows_ack_cleanup[] = {
    {.label = "Put the cup on the scale", .kind = ROW_INFO},
    {.label = "to catch the powder", .kind = ROW_INFO},
    {.label = "Start", .kind = ROW_ACTION, .act = act_ack_cleanup},
};
static const menu_page_t page_ack_cleanup = {.kind = PAGE_DIALOG, .id = "ack/purge", .title = "Purge",
                                             .rows = rows_ack_cleanup, .row_count = ROW_COUNT(rows_ack_cleanup)};

static const menu_row_t rows_charge[3] = {
    {.label = "Start", .kind = ROW_ACTION, .act = act_charge_start},
    {.label = "Target", .kind = ROW_NUMBER, .value = value_target},
    {.label = "Powder", .kind = ROW_CHOICE, .value = value_powder, .count = powder_count, .option = powder_option,
     .current = powder_current, .choose = powder_choose},
};
static const menu_page_t page_charge = {.id = "charge", .title = "Charge", .rows = rows_charge,
                                       .row_count = ROW_COUNT(rows_charge)};

// Between throws, the charge's Change: its target and powder. It closes when the next throw begins
static const menu_page_t page_charge_change = {.id = "charge/change", .title = "Change", .rows = &rows_charge[1],
                                               .row_count = ROW_COUNT(rows_charge) - 1};

/*
 * The set-up's code (ADR-008, gate 4; mockups/setup_12864.png): the code that joins the hotspot, these lines beside
 * it, Back. Once the hotspot gives a phone an address, a dialog in its place: go on with the phone, Back, or the code
 * again
 */
static const menu_row_t rows_setup[] = {
    {.label = "Scan this", .kind = ROW_INFO},
    {.label = "with your", .kind = ROW_INFO},
    {.label = "phone's", .kind = ROW_INFO},
    {.label = "camera.", .kind = ROW_INFO},
};
static const menu_page_t page_setup = {.kind = PAGE_CODE, .id = "setup", .title = "Set up", .rows = rows_setup,
                                       .row_count = ROW_COUNT(rows_setup)};


static void act_join_by_camera(const menu_row_t *row) {
    (void) row;
    push_page(&page_setup);
}


// Notes: each line says what it is, read only, then at the end Wi-Fi turned off or on, or on its own hotspot with no
// network named, the code that joins it
static const menu_row_t rows_wifi[] = {
    {.label = "", .kind = ROW_INFO, .value = value_wifi_mode, .label_now = wifi_mode_label},
    {.label = "Network", .kind = ROW_INFO, .value = value_wifi_station_network, .shown = wifi_has_trouble},
    {.label = "Hotspot", .kind = ROW_INFO, .value = value_wifi_network, .shown = wifi_is_access_point},
    {.label = "Password", .kind = ROW_INFO, .value = value_wifi_key, .shown = wifi_is_access_point},
    {.label = "IP", .kind = ROW_INFO, .value = value_wifi_address},
    {.label = "Name", .kind = ROW_INFO, .value = value_wifi_name},
    {.label = "Signal", .kind = ROW_INFO, .value = value_wifi_signal, .shown = wifi_is_station},
    {.label = "Takes effect at restart", .kind = ROW_INFO, .shown = shown_restart_pending},
    {.label = "Join by camera", .kind = ROW_ACTION, .act = act_join_by_camera, .shown = wifi_code_offered},
    {.label = "Turn Wi-Fi off", .kind = ROW_ACTION, .act = act_wifi_button, .label_now = wifi_button_label,
     .shown = wifi_switch_offered},
};
static const menu_page_t page_wifi = {.kind = PAGE_NOTES, .id = "wifi", .title = "Wi-Fi", .rows = rows_wifi,
                                      .row_count = ROW_COUNT(rows_wifi)};

static const menu_row_t rows_scale[] = {
    {.label = "Reading", .kind = ROW_INFO, .value = value_reading},
    {.label = "Zero", .kind = ROW_ACTION, .act = act_zero},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/scale", .name = "driver",
     .offered = offered_make},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/scale", .name = "baud"},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/scale", .name = "format"},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/charge", .name = "decimal_places"},
    {.label = "Restart to apply", .kind = ROW_ACTION, .act = act_restart, .shown = shown_restart_pending},
};
static const menu_page_t page_scale = {.id = "settings/scale", .title = "Scale", .rows = rows_scale,
                                       .row_count = ROW_COUNT(rows_scale)};

// The charge's two stop thresholds: what a person who only uses the screen sees the effect of (the owner); and step
// 2's two switches, the auto-tuner's (ADR-009 step 2, gate 3) and the forecast's beside it (the owner, 2026-11-27)
static const menu_row_t rows_charge_settings[] = {
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/charge", .name = "fine_stop_threshold"},
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/charge", .name = "coarse_stop_threshold"},
    {.kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle, .path = "settings/charge",
     .name = "auto_tune"},
    {.kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle, .path = "settings/charge",
     .name = "forecast"},
};
static const menu_page_t page_charge_settings = {.id = "settings/charge", .title = "Charge",
                                                 .rows = rows_charge_settings,
                                                 .row_count = ROW_COUNT(rows_charge_settings)};

static const menu_row_t rows_display[] = {
    {.kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle, .path = "settings/display",
     .name = "invert_encoder"},
    {.kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle, .path = "settings/display",
     .name = "invert_display"},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/display", .name = "rotation"},
    {.label = "Backlight", .kind = ROW_CHOICE, .value = value_backlight, .count = backlight_count,
     .option = backlight_option, .current = backlight_current, .choose = backlight_choose},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/display", .name = "charge_view"},
    {.label = "Restart to apply", .kind = ROW_ACTION, .act = act_restart, .shown = shown_restart_pending},
};
static const menu_page_t page_display = {.id = "settings/display", .title = "Display", .rows = rows_display,
                                         .row_count = ROW_COUNT(rows_display)};

// The servo gate: on or off, the shutters moved, and their ends set while the servo shows them
#if FEATURE_SERVO_GATE
static const menu_row_t rows_gate[] = {
    {.kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle, .path = "settings/gate", .name = "enable"},
    {.label = "Shutters", .kind = ROW_ACTION, .value = value_gate, .act = act_gate},
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/gate", .name = "shutter0_close_duty",
     .stepped = gate_show_end},
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/gate", .name = "shutter0_open_duty",
     .stepped = gate_show_end},
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/gate", .name = "shutter1_close_duty",
     .stepped = gate_show_end},
    {.kind = ROW_STEP, .value = value_setting_step, .path = "settings/gate", .name = "shutter1_open_duty",
     .stepped = gate_show_end},
};
static const menu_page_t page_gate = {.id = "settings/gate", .title = "Servo gate", .rows = rows_gate,
                                      .row_count = ROW_COUNT(rows_gate)};
#endif

// Each motor's direction: a motor wired the other way, found at the bench
static const menu_row_t rows_motors[] = {
    {.label = "Coarse direction", .kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle,
     .path = "settings/motors/coarse", .name = "invert_direction"},
    {.label = "Fine direction", .kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle,
     .path = "settings/motors/fine", .name = "invert_direction"},
};
static const menu_page_t page_motors = {.id = "settings/motors", .title = "Motors", .rows = rows_motors,
                                        .row_count = ROW_COUNT(rows_motors)};

// A list: the facts to read, then the channel set and the check
static const menu_row_t rows_firmware[] = {
    {.label = "Version", .kind = ROW_INFO, .value = value_version},
    {.label = "Update", .kind = ROW_INFO, .value = value_update_state, .shown = shown_on_station},
    {.label = "Update", .kind = ROW_INFO, .value = value_update_needs_network, .shown = shown_not_on_station},
    {.label = "Last update", .kind = ROW_INFO, .value = value_last_update, .shown = shown_last_update},

    {.label = "Board", .kind = ROW_INFO, .value = value_board},
    {.label = "Unit", .kind = ROW_INFO, .value = value_unit_id},
    {.label = "Build", .kind = ROW_INFO, .value = value_build},
    {.kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/update", .name = "channel"},
    {.label = "Check for update", .kind = ROW_ACTION, .act = act_check_update, .label_now = check_update_label,
     .shown = shown_on_station},
};
static const menu_page_t page_firmware = {.id = "settings/firmware", .title = "Firmware", .rows = rows_firmware,
                                          .row_count = ROW_COUNT(rows_firmware)};

/*
 * The offer, notes: what the release says and what to know before installing, then Install, reached only by
 * scrolling through them (the owner, 2026-11-17). Back is Later, throughout
 */
static const menu_row_t rows_update[] = {
    {.label = "", .kind = ROW_INFO, .label_now = offer_heading, .bold = true},
    {.label = "", .kind = ROW_INFO, .label_now = offer_note},
    {.label = "", .kind = ROW_INFO, .label_now = offer_from_bench, .shown = shown_update_from_bench},
    {.label = "", .kind = ROW_INFO, .label_now = update_went_back_label, .shown = shown_update_went_back},
    {.label = "The last try failed: see the web page.", .kind = ROW_INFO, .shown = shown_update_failed},
    {.label = "Before you install", .kind = ROW_INFO, .bold = true},
    {.label = "Keep the power on until the unit has restarted.", .kind = ROW_INFO},
    {.label = "Your settings are kept.", .kind = ROW_INFO},
    {.label = "Install", .kind = ROW_ACTION, .act = act_update_install},
};
static const menu_page_t page_update = {.kind = PAGE_NOTES, .id = "update", .title = "Update", .rows = rows_update,
                                        .row_count = ROW_COUNT(rows_update)};

static const menu_row_t rows_factory_reset[] = {
    {.label = "Erase every setting?", .kind = ROW_INFO},
    {.label = "It cannot be undone", .kind = ROW_INFO},
    {.label = "Erase", .kind = ROW_ACTION, .act = act_erase},
};
static const menu_page_t page_factory_reset = {.kind = PAGE_DIALOG, .id = "settings/factory_reset",
                                               .title = "Factory reset", .rows = rows_factory_reset,
                                               .row_count = ROW_COUNT(rows_factory_reset),
                                               .cancel_first = true};

// ADR-009 step 1: the calibration, its three parts each started alone from its row (Settings > Calibration)
static void calibration_start_row(calibration_part_t part) {
    if (motors_fault_shown("Calibration")) {
        return;
    }
    if (calibration_request_start(part, -1, ACTIVITY_BY_BUTTON) == CALIBRATION_START_DRIVER_FAULT) {
        push_message(motors_get_fault(), "Calibration", "cannot start");
    }
}


static void act_calibrate_cup(const menu_row_t *row) {
    (void) row;
    calibration_start_row(CALIBRATION_PART_CUP);
}


static void act_calibrate_scale(const menu_row_t *row) {
    (void) row;
    calibration_start_row(CALIBRATION_PART_SCALE);
}


static void act_calibrate_powder(const menu_row_t *row) {
    (void) row;
    calibration_start_row(CALIBRATION_PART_POWDER);
}


static void value_cup_capacity(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    float capacity = charge_mode_config.eeprom_charge_mode_data.cup_capacity;
    if (capacity < 0.0f) {
        snprintf(out, room, "Not done");
        return;
    }
    char weight[16];
    format_weight(capacity, weight, sizeof(weight));
    snprintf(out, room, "%s%s%s", weight, unit_text()[0] != '\0' ? " " : "", unit_text());
}


static void value_scale_calibrated(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", scale_config.persistent_config.settle_ms < 0.0f ? "Not done" : "Done");
}


static void value_selected_powder(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", profile_get_name(profile_get_selected_idx()));
}


// Offered on the main menu until the cup and the scale are calibrated
static bool calibration_offered(void) {
    return charge_mode_config.eeprom_charge_mode_data.cup_capacity < 0.0f ||
           scale_config.persistent_config.settle_ms < 0.0f;
}


static const menu_row_t rows_calibration[] = {
    {.label = "Cup", .kind = ROW_ACTION, .act = act_calibrate_cup, .value = value_cup_capacity},
    {.label = "Scale", .kind = ROW_ACTION, .act = act_calibrate_scale, .value = value_scale_calibrated},
    {.label = "Powder", .kind = ROW_ACTION, .act = act_calibrate_powder, .value = value_selected_powder},
};
static const menu_page_t page_calibration = {.id = "settings/calibration", .title = "Calibration",
                                             .rows = rows_calibration, .row_count = ROW_COUNT(rows_calibration)};

static const menu_row_t rows_settings[] = {
    {.label = "Calibration", .kind = ROW_PAGE, .page = &page_calibration},
    {.label = "Scale", .kind = ROW_PAGE, .page = &page_scale},
    {.label = "Charge", .kind = ROW_PAGE, .page = &page_charge_settings},
    {.label = "Display", .kind = ROW_PAGE, .page = &page_display},
#if FEATURE_SERVO_GATE
    {.label = "Servo gate", .kind = ROW_PAGE, .page = &page_gate},
#endif
    {.label = "Motors", .kind = ROW_PAGE, .page = &page_motors},
    {.label = "Firmware", .kind = ROW_PAGE, .page = &page_firmware},
    {.label = "Restart", .kind = ROW_ACTION, .act = act_restart, .label_now = restart_label},
    {.label = "Factory reset", .kind = ROW_PAGE, .page = &page_factory_reset},
};
static const menu_page_t page_settings = {.id = "settings", .title = "Settings", .rows = rows_settings,
                                          .row_count = ROW_COUNT(rows_settings)};

// "OpenTrickler", or the motor driver that is not OK (ADR-012, step 6)
static const char *main_title(void) {
    const char *fault = motors_get_fault();
    return fault != NULL ? fault : "OpenTrickler";
}

static const menu_row_t rows_main[] = {
    {.label = "Set up this unit", .kind = ROW_PAGE, .page = &page_setup, .shown = setup_offered},
    {.label = "Update available", .kind = ROW_PAGE, .page = &page_update, .label_now = update_row_label,
     .shown = shown_update_found},
    {.label = "Calibrate this unit", .kind = ROW_PAGE, .page = &page_calibration, .shown = calibration_offered},
    {.label = "Charge", .kind = ROW_PAGE, .page = &page_charge},
    {.label = "Purge", .kind = ROW_ACTION, .act = act_cleanup_start},
    {.label = "Wi-Fi", .kind = ROW_PAGE, .page = &page_wifi},
    {.label = "Settings", .kind = ROW_PAGE, .page = &page_settings},
};
static const menu_page_t page_main = {.id = "main", .title = "OpenTrickler", .title_now = main_title,
                                      .rows = rows_main, .row_count = ROW_COUNT(rows_main)};

// Every page, for the console and the menu's tour; a page's parents are found through the tree
static const menu_page_t *const all_pages[] = {
    &page_main, &page_charge, &page_wifi, &page_settings, &page_calibration, &page_scale, &page_charge_settings,
    &page_display,
#if FEATURE_SERVO_GATE
    &page_gate,
#endif
    &page_motors, &page_firmware,
    &page_factory_reset, &page_ack_charge, &page_ack_cleanup, &page_update, &page_setup,
};
#define ALL_PAGES_COUNT (sizeof(all_pages) / sizeof(all_pages[0]))


// --- Cards and notes: their rows as lines, wrapped to the view's width

void menu_nav_set_text_width(menu_text_width_fn width, int line_width, uint8_t lines_shown) {
    nav_text_width = width;
    nav_line_width = line_width;
    nav_lines_shown = lines_shown;
}


static int text_width(const char *text, bool bold) {
    // Before the view gives its measure (the console alone): about 5 pixels a character
    return nav_text_width != NULL ? nav_text_width(text, bold) : (int) strlen(text) * 5;
}


// A paragraph's lines, broken at spaces to the view's width, into lines[count...]; the new count
static uint8_t wrap_into(const char *text, bool bold, char lines[][MENU_TEXT], bool *bolds, uint8_t count) {
    char line[MENU_TEXT] = "";
    while (*text != '\0' && count < MENU_ROWS_MAX) {
        while (*text == ' ') {
            text += 1;
        }
        size_t word = strcspn(text, " ");
        if (word == 0) {
            break;
        }
        char trial[MENU_TEXT];
        snprintf(trial, sizeof(trial), "%s%s%.*s", line, line[0] != '\0' ? " " : "", (int) word, text);
        bool too_wide = text_width(trial, bold) > nav_line_width || strlen(line) + 1 + word >= MENU_TEXT;
        if (too_wide && line[0] != '\0') {
            snprintf(lines[count], MENU_TEXT, "%s", line);
            bolds[count] = bold;
            count += 1;
            line[0] = '\0';
            continue;
        }
        snprintf(line, sizeof(line), "%s", trial);     // a word wider than a line is cut there
        text += word;
    }
    if (line[0] != '\0' && count < MENU_ROWS_MAX) {
        snprintf(lines[count], MENU_TEXT, "%s", line);
        bolds[count] = bold;
        count += 1;
    }
    return count;
}


// The room between a key and its value on a line, in the view's pixels
#define NAV_PAIR_GAP 6
// The values of lines only counted, not shown: one buffer, off the menu task's stack (only that task navigates)
static char nav_values_unused[MENU_ROWS_MAX][MENU_TEXT];

/*
 * A card's or notes' lines: each shown info row a line, its label (the key) at the left and its value at the right
 * (values[n], "" for none). A key and a value too wide for one line take two, the value right on the second; a row
 * with only one of them is wrapped at the left. On notes, the action row is the button at the end (*button, NULL
 * when none is shown). The lines' count
 */
static uint8_t page_lines(const menu_page_t *page, char lines[][MENU_TEXT], char values[][MENU_TEXT], bool *bolds,
                          const menu_row_t **button) {
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t shown = shown_rows(page, rows);
    uint8_t count = 0;
    *button = NULL;
    for (uint8_t idx = 0; idx < shown; idx += 1) {
        const menu_row_t *row = rows[idx];
        if (row->kind == ROW_ACTION) {
            *button = row;
            continue;
        }
        char value[MENU_TEXT] = "";
        if (row->value != NULL) {
            row->value(row, value, sizeof(value));
        }
        const char *label = row->label_now != NULL ? row->label_now() : row->label;
        uint8_t from = count;
        int value_w = text_width(value, false);
        if (label[0] != '\0' && value[0] != '\0' && value_w <= nav_line_width) {
            bool one_line = text_width(label, row->bold) + NAV_PAIR_GAP + value_w <= nav_line_width;
            if (!one_line) {
                count = wrap_into(label, row->bold, lines, bolds, count);
                for (uint8_t idx = from; idx < count; idx += 1) {
                    values[idx][0] = '\0';
                }
            }
            if (count < MENU_ROWS_MAX) {
                snprintf(lines[count], MENU_TEXT, "%s", one_line ? label : "");
                bolds[count] = row->bold;
                snprintf(values[count], MENU_TEXT, "%s", value);
                count += 1;
            }
            continue;
        }
        // One of the two, or a value wider than a line: wrapped at the left, the value under its key
        count = wrap_into(label[0] != '\0' ? label : value, row->bold, lines, bolds, count);
        if (label[0] != '\0' && value[0] != '\0') {
            count = wrap_into(value, false, lines, bolds, count);
        }
        for (uint8_t idx = from; idx < count; idx += 1) {
            values[idx][0] = '\0';
        }
    }
    return count;
}


// The first line a card or notes may show: the button, on notes, is a line of its own after the rest
static uint8_t page_last_first(const menu_page_t *page) {
    char lines[MENU_ROWS_MAX][MENU_TEXT];
    bool bolds[MENU_ROWS_MAX];
    const menu_row_t *button;
    uint8_t count = page_lines(page, lines, nav_values_unused, bolds, &button) + (button != NULL ? 1 : 0);
    return count > nav_lines_shown ? (uint8_t) (count - nav_lines_shown) : 0;
}


// --- The number editor: the target, digit by digit as the menu always set it

static uint32_t digits_value(const frame_t *frame) {
    uint32_t value = 0;
    for (uint8_t idx = 0; idx < MENU_EDIT_DIGITS; idx += 1) {
        value = value * 10 + frame->digits[idx];
    }
    return value;
}


static void push_edit(const menu_row_t *row) {
    frame_t *frame = push(FRAME_EDIT);
    frame->row = row;
    frame->decimals = charge_decimals() == DP_3 ? 3 : 2;
    float target = charge_mode_get_target();
    uint32_t scaled = target > 0.0f ? (uint32_t) lroundf(target * (frame->decimals == 3 ? 1000.0f : 100.0f)) : 0;
    for (int idx = MENU_EDIT_DIGITS - 1; idx >= 0; idx -= 1) {
        frame->digits[idx] = (uint8_t) (scaled % 10);
        scaled /= 10;
    }
    frame->digit_at = 0;
}


static void edit_confirm(frame_t *frame) {
    float target = (float) digits_value(frame) / (frame->decimals == 3 ? 1000.0f : 100.0f);
    // As PATCH /api/v1/charge: one target, refused while a throw pours
    activity_lock();
    bool in_throw = activity_in_throw();
    bool set = !in_throw && target > 0.0f && charge_mode_set_target(target);
    activity_unlock();
    pop();
    if (!set) {
        push_message("Not set", in_throw ? "not while a throw pours" : "not a target the scale shows", NULL);
    }
}


/*
 * The editor, as the old menu's: the knob moves the focus over the digits, then Cancel and Set; a click on a digit
 * starts setting it (the knob then turns its value) and a second click ends that; a click on Cancel or Set does it.
 * Back ends a digit's setting, or leaves the value unchanged
 */
static void edit_key(frame_t *frame, menu_key_t key) {
    const int8_t last = MENU_EDIT_DIGITS + 1;      // Set
    if (frame->editing) {
        uint8_t *digit = &frame->digits[frame->digit_at];
        if (key == MENU_KEY_NEXT) {
            *digit = (uint8_t) ((*digit + 1) % 10);
        }
        else if (key == MENU_KEY_PREV) {
            *digit = (uint8_t) ((*digit + 9) % 10);
        }
        else {
            frame->editing = false;
        }
        return;
    }
    switch (key) {
        case MENU_KEY_NEXT:
            frame->digit_at = (int8_t) (frame->digit_at < last ? frame->digit_at + 1 : last);
            break;
        case MENU_KEY_PREV:
            frame->digit_at = (int8_t) (frame->digit_at > 0 ? frame->digit_at - 1 : 0);
            break;
        case MENU_KEY_ENTER:
            if (frame->digit_at == last) {
                edit_confirm(frame);
            }
            else if (frame->digit_at == MENU_EDIT_DIGITS) {
                pop();      // Cancel: left unchanged
            }
            else {
                frame->editing = true;
            }
            break;
        case MENU_KEY_BACK:
            pop();          // left unchanged
            break;
    }
}


// --- A job's screen: what a click does, by the job and its state (menu_design.md, "The jobs' screens")

typedef enum {
    JOB_ACTION_NONE,
    JOB_ACTION_ZERO,
    JOB_ACTION_CHANGE,
    JOB_ACTION_ACKNOWLEDGE,
} job_action_t;


// What a click does on the charge's screen now: its right softkey
static job_action_t charge_action(void) {
    charge_status_t charge;
    charge_mode_get_status(&charge);
    if (strcmp(charge.state, "fault") == 0) {
        return JOB_ACTION_ACKNOWLEDGE;
    }
    // A zero while the charge waits for zero, to tare the cup on the pan. Not while it waits for the cup's return:
    // the charge takes a reading of 0 as the cup back, so a zero with the pan bare would start a throw on it. The API
    // refuses that zero for the same reason (review of ADR-010's gate 4)
    if (strcmp(charge.state, "zeroing") == 0) {
        return JOB_ACTION_ZERO;
    }
    // Between throws only: the API refuses a target or a powder in a throw
    bool between = strcmp(charge.state, "waiting_for_cup_return") == 0 ||
                   strcmp(charge.state, "waiting_for_cup_removal") == 0;
    if (between && !activity_in_throw()) {
        return JOB_ACTION_CHANGE;
    }
    return JOB_ACTION_NONE;
}


static const char *job_action_name(job_action_t action) {
    switch (action) {
        case JOB_ACTION_ZERO: return "Zero";
        case JOB_ACTION_CHANGE: return "Change";
        case JOB_ACTION_ACKNOWLEDGE: return "OK";
        case JOB_ACTION_NONE:
        default: return "";
    }
}


// The cleanup's focus: its title (Back, as on every page), its rows, then its softkeys
enum {
    CLEANUP_TITLE = -1,
    CLEANUP_SPEED = 0,
    CLEANUP_GATE,
    CLEANUP_STOP,
    CLEANUP_ZERO,
};


/*
 * The cleanup as one system (ADR-010, gate 6): one value, the speed, set live by a click, the turn (1 rev/s a
 * detent, through 0 into reverse; 0 stands still) and a click; the gate opened or closed by a click, as the API does;
 * Stop; Zero. Its title is Back, turned up to from Speed as on every page: leaving it ends it. The reset button stops
 * it, while the speed is set too
 */
static const char *gate_move(gate_ratio_t ratio);

static void cleanup_key(frame_t *frame, menu_key_t key) {
    cleanup_status_t cleanup;
    cleanup_mode_get_status(&cleanup);
    if (strcmp(cleanup.state, "fault") == 0) {
        // Stop and OK, which both end it: the knob moves between them
        if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
            frame->softkey = key == MENU_KEY_NEXT ? 1 : 0;
        }
        else {
            activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_BUTTON);     // acknowledged
        }
        return;
    }
    if (key == MENU_KEY_BACK) {
        activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_BUTTON);
        return;
    }
    if (frame->editing) {
        if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
            cleanup_mode_set_speed(cleanup.speed_rps + (key == MENU_KEY_NEXT ? 1.0f : -1.0f), NAN);
        }
        else {
            frame->editing = false;
        }
        return;
    }
    if (key == MENU_KEY_NEXT) {
        frame->focus = (int8_t) (frame->focus < CLEANUP_ZERO ? frame->focus + 1 : CLEANUP_ZERO);
        return;
    }
    if (key == MENU_KEY_PREV) {
        frame->focus = (int8_t) (frame->focus > CLEANUP_TITLE ? frame->focus - 1 : CLEANUP_TITLE);
        return;
    }
    switch (frame->focus) {
        case CLEANUP_TITLE:
            activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_BUTTON);
            break;
        case CLEANUP_SPEED:
            frame->editing = true;
            break;
        case CLEANUP_GATE: {
            const char *why = gate_move(servo_gate_state() == GATE_OPEN ? SERVO_GATE_RATIO_CLOSED :
                                        SERVO_GATE_RATIO_OPEN);
            if (why != NULL) {
                push_message("Not moved", why, NULL);
            }
            break;
        }
        case CLEANUP_STOP:
            activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_BUTTON);
            break;
        default:
            scale_ask_zero();
            break;
    }
}


/*
 * The calibration's job (ADR-009 step 1, gate 4): each step its own screen, as mocked (calibration_12864.py). The
 * steps that ask the person are dialogs, Cancel or Stop at the left and the answer at the right; the cup's fill is a
 * job whose turn pours (each detent a bounded pour of the coarse tube) and whose click is Full (the owner,
 * 2026-11-25); the records found are notes ending in Keep or Save; the
 * others a job's screen, the weight or how far it is as the figure. Back stops it at any step, discarding what was
 * not kept.
 */
#define CALIBRATION_NOTES_LINES 4       // the 12864's lines of notes (menu_view_12864.c)

// The right softkey's word at a step and the answer it sends; NULL where the step waits for nothing of the person
static const char *calibration_answer_label(const calibration_status_t *cal, const char **answer) {
    static const struct {
        calibration_step_t step;
        const char *label;
        const char *answer;
    } answers[] = {
        {CALIBRATION_STEP_FILL, "It's full", "full"}, {CALIBRATION_STEP_KEEP, "Keep", "keep"},
        {CALIBRATION_STEP_EMPTY, "Done", "done"}, {CALIBRATION_STEP_FAILED, "OK", "done"},
        {CALIBRATION_STEP_PAUSED, "Go on", "go_on"}, {CALIBRATION_STEP_RESULT, "Save", "save"},
    };
    if (cal->step == CALIBRATION_STEP_CUP_ON) {
        if (answer != NULL) {
            *answer = "next";
        }
        return cal->part == CALIBRATION_PART_CUP ? "Next" : "Start";
    }
    for (size_t idx = 0; idx < sizeof(answers) / sizeof(answers[0]); idx += 1) {
        if (answers[idx].step == cal->step) {
            if (answer != NULL) {
                *answer = answers[idx].answer;
            }
            return answers[idx].label;
        }
    }
    return NULL;
}


// The left softkey: Cancel before anything is turned, Stop once it pours; none where the step only ends
static const char *calibration_left_label(calibration_step_t step) {
    switch (step) {
        case CALIBRATION_STEP_RUNNING:
        case CALIBRATION_STEP_PAUSED:
            return "Stop";
        case CALIBRATION_STEP_EMPTY:
        case CALIBRATION_STEP_FAILED:
        case CALIBRATION_STEP_RESULT:
            return "";
        default:
            return "Cancel";
    }
}


// The result's lines: the scale's records, or the powder's least speeds beside what they were
static uint8_t calibration_result_lines(const calibration_status_t *cal, char label[][MENU_TEXT],
                                        char value[][MENU_TEXT], bool *bold) {
    uint8_t count = 0;
    float seconds;
    float poured;
    if (cal->part == CALIBRATION_PART_SCALE) {
        snprintf(label[count], MENU_TEXT, "Settle time");
        snprintf(value[count++], MENU_TEXT, "%.2f s", (double) cal->scale.settle_s);
        if (cal->scale.lag_count > 0) {
            snprintf(label[count], MENU_TEXT, "Lag, slow flow");
            snprintf(value[count++], MENU_TEXT, "%.2f s", (double) cal->scale.lag_s[0]);
            snprintf(label[count], MENU_TEXT, "Lag, fast flow");
            snprintf(value[count++], MENU_TEXT, "%.2f s", (double) cal->scale.lag_s[cal->scale.lag_count - 1]);
        }
        seconds = cal->scale.seconds;
        poured = cal->scale.poured;
    }
    else {
        const calibration_speeds_record_t *tubes[2] = {&cal->fine, &cal->coarse};
        const float was[2] = {cal->was_fine_rps, cal->was_coarse_rps};
        for (int idx = 0; idx < 2; idx += 1) {
            bold[count] = true;
            snprintf(label[count], MENU_TEXT, "%s", idx == 0 ? "Fine tube" : "Coarse tube");
            value[count++][0] = '\0';
            snprintf(label[count], MENU_TEXT, "Minimum");
            if (isnan(tubes[idx]->least_rps)) {
                snprintf(value[count++], MENU_TEXT, "Not found");
            }
            else {
                snprintf(value[count++], MENU_TEXT, "%.2f rev/s", (double) tubes[idx]->least_rps);
            }
            snprintf(label[count], MENU_TEXT, "Was");
            snprintf(value[count++], MENU_TEXT, "%.2f rev/s", (double) was[idx]);
            if (idx == 0) {
                // Its weight a turn, kept as step 2's first figure for this powder
                snprintf(label[count], MENU_TEXT, "A turn");
                if (isfinite(cal->fine.stream_per_turn) && cal->fine.stream_per_turn > 0.0f) {
                    snprintf(value[count++], MENU_TEXT, "%.3f%s%s", (double) cal->fine.stream_per_turn,
                             unit_text()[0] != '\0' ? " " : "", unit_text());
                }
                else {
                    snprintf(value[count++], MENU_TEXT, "Not found");
                }
            }
        }
        seconds = cal->fine.seconds + cal->coarse.seconds;
        poured = cal->fine.poured + cal->coarse.poured;
    }
    snprintf(label[count], MENU_TEXT, "Took");
    snprintf(value[count++], MENU_TEXT, "%.1f min, %.0f%s%s", (double) (seconds / 60.0f), (double) poured,
             unit_text()[0] != '\0' ? " " : "", unit_text());
    return count;
}


// The notes' first line when scrolled to their end, their button a line of its own
static uint8_t calibration_last_first(const calibration_status_t *cal) {
    char label[MENU_ROWS_MAX][MENU_TEXT];
    char value[MENU_ROWS_MAX][MENU_TEXT];
    bool bold[MENU_ROWS_MAX] = {false};
    int lines = calibration_result_lines(cal, label, value, bold) + 1;
    return (uint8_t) (lines > CALIBRATION_NOTES_LINES ? lines - CALIBRATION_NOTES_LINES : 0);
}


// A new step: its default focus, the answer where there is one
static void calibration_step_seen(frame_t *frame, const calibration_status_t *cal) {
    if (frame->action_seen == (uint8_t) cal->step) {
        return;
    }
    frame->action_seen = (uint8_t) cal->step;
    frame->editing = false;
    frame->first = 0;
    frame->at_end = false;
    frame->focus = 0;
    frame->softkey = calibration_answer_label(cal, NULL) != NULL ? 1 : 0;
}


static void calibration_key(frame_t *frame, menu_key_t key) {
    calibration_status_t cal;
    calibration_get_status(&cal);
    calibration_step_seen(frame, &cal);
    const char *answer = NULL;
    const char *label = calibration_answer_label(&cal, &answer);
    if (key == MENU_KEY_BACK) {
        activity_request_stop(ACTIVITY_CALIBRATION, ACTIVITY_BY_BUTTON);
        return;
    }

    if (cal.step == CALIBRATION_STEP_FILL) {
        // The knob turns the coarse tube: right pours, left reverses, a detent a bounded turn; the click is Full,
        // Back cancels (above: the title's "<")
        if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
            calibration_pour_detent(key == MENU_KEY_NEXT);
        }
        else if (key == MENU_KEY_ENTER) {
            calibration_answer("full", ACTIVITY_BY_BUTTON);
        }
        return;
    }

    if (cal.step == CALIBRATION_STEP_RESULT) {
        // As on a notes page: turned a line at a time, or a click turns the page, to the end, so the result is read
        // on the way; Keep or Save is reached the moment it shows, and a click there keeps them
        uint8_t last = calibration_last_first(&cal);
        if (key == MENU_KEY_NEXT) {
            frame->first = (uint8_t) (frame->first < last ? frame->first + 1 : last);
        }
        else if (key == MENU_KEY_PREV) {
            frame->first = (uint8_t) (frame->first > 0 ? frame->first - 1 : 0);
        }
        else if (key == MENU_KEY_ENTER) {
            if (frame->first >= last) {
                calibration_save(ACTIVITY_BY_BUTTON);
            }
            else {
                frame->first = (uint8_t) (frame->first + CALIBRATION_NOTES_LINES < last ?
                                          frame->first + CALIBRATION_NOTES_LINES : last);
            }
        }
        frame->at_end = frame->first >= last;
        return;
    }

    // A dialog or a job's screen: the knob between the softkeys there are, a click does the focused one
    bool left = calibration_left_label(cal.step)[0] != '\0';
    if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
        frame->softkey = (int8_t) (label == NULL ? 0 : (!left ? 1 : (key == MENU_KEY_NEXT ? 1 : 0)));
        return;
    }
    if (key == MENU_KEY_ENTER) {
        if (frame->softkey == 1 && answer != NULL) {
            calibration_answer(answer, ACTIVITY_BY_BUTTON);
        }
        else if (left) {
            activity_request_stop(ACTIVITY_CALIBRATION, ACTIVITY_BY_BUTTON);
        }
    }
}

/*
 * A job's keys. Its softkeys are Stop at the left and its action at the right; the knob moves between them where
 * the turn is free, and a click does the focused one. Back stops it (but an update while it installs)
 */
static void job_key(frame_t *frame, menu_key_t key) {
    if (frame->job == ACTIVITY_UPDATE) {
        update_status_t update;
        update_status(&update);
        if ((key == MENU_KEY_BACK || key == MENU_KEY_ENTER) && update.state != UPDATE_INSTALLING) {
            activity_request_stop(ACTIVITY_UPDATE, ACTIVITY_BY_BUTTON);     // Stop, its one softkey
        }
        return;
    }
    if (frame->job == ACTIVITY_CLEANUP) {
        cleanup_key(frame, key);
        return;
    }
    if (frame->job == ACTIVITY_CALIBRATION) {
        calibration_key(frame, key);
        return;
    }
    if (key == MENU_KEY_BACK || (key == MENU_KEY_ENTER && frame->softkey == 0)) {
        activity_request_stop(frame->job, ACTIVITY_BY_BUTTON);
        return;
    }

    if (frame->job == ACTIVITY_CHARGE) {
        if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
            // To the action only when there is one
            frame->softkey = key == MENU_KEY_NEXT && charge_action() != JOB_ACTION_NONE ? 1 : 0;
            return;
        }
        if (key == MENU_KEY_ENTER) {
            switch (charge_action()) {
                case JOB_ACTION_ZERO: {
                    // Under the lock the charge starts its pour under, and only while it still waits for zero: a zero
                    // landing as the pour begins would pour a whole target on top (as POST /api/v1/scale/zero)
                    activity_lock();
                    charge_status_t now;
                    charge_mode_get_status(&now);
                    if (strcmp(now.state, "zeroing") == 0) {
                        scale_ask_zero();
                    }
                    activity_unlock();
                    break;
                }
                case JOB_ACTION_CHANGE:
                    push_page(&page_charge_change);
                    break;
                case JOB_ACTION_ACKNOWLEDGE:
                    activity_request_stop(ACTIVITY_CHARGE, ACTIVITY_BY_BUTTON);
                    break;
                case JOB_ACTION_NONE:
                    break;
            }
        }
    }
}


// --- A choice's page: every option offered, the current one marked, the focus opening on it (the owner, 2026-11-17:
// a choice is seen whole, never turned through blind in its row)

static void push_choice(const menu_row_t *row) {
    frame_t *frame = push(FRAME_CHOICE);
    frame->row = row;
    int now = row_choice_now(row);
    int shown = 0;
    for (int idx = 0; idx < row_choice_count(row); idx += 1) {
        if (row_choice_label(row, idx) == NULL) {
            continue;
        }
        if (idx == now) {
            frame->focus = (int8_t) shown;
        }
        shown += 1;
    }
}


// The option at the shown index, or -1
static int choice_at(const menu_row_t *row, int shown_idx) {
    int shown = 0;
    for (int idx = 0; idx < row_choice_count(row); idx += 1) {
        if (row_choice_label(row, idx) == NULL) {
            continue;
        }
        if (shown == shown_idx) {
            return idx;
        }
        shown += 1;
    }
    return -1;
}


static int choice_shown_count(const menu_row_t *row) {
    int shown = 0;
    for (int idx = 0; idx < row_choice_count(row); idx += 1) {
        shown += row_choice_label(row, idx) != NULL ? 1 : 0;
    }
    return shown;
}


// --- A number set in place (ADR-010, gate 6): stepped live in its row, stored at the click

static void inplace_begin(frame_t *frame, const menu_row_t *row) {
    if (!settings_read_text(row->path, row->name, frame->edit_was, sizeof(frame->edit_was))) {
        return;
    }
    frame->editing = true;
}


// A number being set, put back as it was and no longer set: as Back does, with no message
static void inplace_cancel(frame_t *frame) {
    if (frame->kind != FRAME_PAGE || !frame->editing) {
        return;
    }
    frame->editing = false;
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t shown = shown_rows(frame->page, rows);
    if (frame->focus < 0 || frame->focus >= shown) {
        return;
    }
    const menu_row_t *row = rows[frame->focus];
    const char *why = settings_write(row->path, row->name, frame->edit_was);
    if (why != NULL) {
        printf("Menu: %s %s not put back: %s\n", row->path, row->name, why);
    }
    else if (row->stepped != NULL) {
        row->stepped(row);
    }
}


static void inplace_refused(const char *why) {
    if (why != NULL) {
        push_message("Not changed", why, NULL);
    }
}


// The keys while a value is set in place: the turn changes it, a click stores it, Back puts it back
static void inplace_key(frame_t *frame, const menu_row_t *row, menu_key_t key) {
    // A number: each detent written live through the API's checks, stored at the click
    const setting_field_t *field = settings_field(row->path, row->name);
    if (key == MENU_KEY_NEXT || key == MENU_KEY_PREV) {
        char text[16];
        if (field == NULL || !settings_read_text(row->path, row->name, text, sizeof(text))) {
            return;
        }
        float value = strtof(text, NULL) + (key == MENU_KEY_NEXT ? field->edit_step : -field->edit_step);
        value = !isnan(field->min) && value < field->min ? field->min : value;
        value = !isnan(field->max) && value > field->max ? field->max : value;
        snprintf(text, sizeof(text), "%.*f", step_decimals(field->edit_step) + 1, (double) value);
        const char *why = settings_write(row->path, row->name, text);
        if (why != NULL) {
            frame->editing = false;
            inplace_refused(why);
        }
        else if (row->stepped != NULL) {
            row->stepped(row);
        }
        return;
    }
    frame->editing = false;
    if (key == MENU_KEY_ENTER) {
        inplace_refused(settings_store(row->path));
    }
    else {
        const char *why = settings_write(row->path, row->name, frame->edit_was);
        if (why == NULL && row->stepped != NULL) {
            row->stepped(row);
        }
        inplace_refused(why);
    }
}


// --- Keys on a list: a page, a message

// Whether a page's shown row at idx does something: focus passes over the rest (the skill, rule 12)
static bool row_acts(const frame_t *frame, int idx) {
    if (frame->kind != FRAME_PAGE) {
        return true;
    }
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t count = shown_rows(frame->page, rows);
    return idx < count && rows[idx]->kind != ROW_INFO;
}


static void list_key(frame_t *frame, menu_key_t key, int count) {
    if (frame->kind == FRAME_PAGE && frame->editing) {
        const menu_row_t *rows[MENU_ROWS_MAX];
        uint8_t shown = shown_rows(frame->page, rows);
        if (frame->focus >= 0 && frame->focus < shown) {
            inplace_key(frame, rows[frame->focus], key);
            return;
        }
        frame->editing = false;
    }
    switch (key) {
        case MENU_KEY_NEXT:
            for (int idx = frame->focus + 1; idx < count; idx += 1) {
                if (row_acts(frame, idx)) {
                    frame->focus = (int8_t) idx;
                    break;
                }
            }
            return;
        case MENU_KEY_PREV: {
            int idx = frame->focus - 1;
            while (idx >= 0 && !row_acts(frame, idx)) {
                idx -= 1;
            }
            frame->focus = (int8_t) (idx < 0 ? -1 : idx);      // -1: the title, which is Back; it stops there
            return;
        }
        case MENU_KEY_BACK:
            pop();
            return;
        case MENU_KEY_ENTER:
            break;
    }

    if (frame->focus < 0) {
        pop();      // the title is Back
        return;
    }
    if (frame->kind == FRAME_MESSAGE) {
        pop();
        return;
    }
    if (frame->kind == FRAME_CHOICE) {
        const menu_row_t *row = frame->row;
        int idx = choice_at(row, frame->focus);
        const char *why = idx >= 0 ? row_choose(row, idx) : "no such choice";
        pop();
        if (why != NULL) {
            push_message("Not changed", why, NULL);
        }
        return;
    }

    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t shown = shown_rows(frame->page, rows);
    if (frame->focus >= shown) {
        return;
    }
    const menu_row_t *row = rows[frame->focus];
    switch (row->kind) {
        case ROW_PAGE:
            push_page(row->page);
            break;
        case ROW_CHOICE:
            push_choice(row);
            break;
        case ROW_STEP:
            inplace_begin(frame, row);
            break;
        case ROW_NUMBER:
            push_edit(row);
            break;
        case ROW_ACTION:
        case ROW_TOGGLE:
            row->act(row);
            break;
        case ROW_INFO:
            break;
    }
}


// A card, a dialog or notes: their own keys
static void page_kind_key(frame_t *frame, menu_key_t key) {
    const menu_page_t *page = frame->page;
    if (key == MENU_KEY_BACK && page->kind == PAGE_CODE && frame->code_shown) {
        frame->code_shown = false;      // the code asked for: Back goes back to the stage
        return;
    }
    if (key == MENU_KEY_BACK) {
        pop();
        return;
    }
    if (page->kind == PAGE_CODE) {
        // The set-up's stage: Back leaves it for the menu, the set-up going on on the phone; the code asked for goes
        // back to the stage. Where there are two softkeys the knob moves between them; OK at the end, to the main menu
        setup_stage_t stage = setup_stage(frame);
        bool two = stage == STAGE_CHOOSE || stage == STAGE_PASSWORD;
        if ((key == MENU_KEY_NEXT || key == MENU_KEY_PREV) && two) {
            frame->softkey = key == MENU_KEY_NEXT ? 1 : 0;
        }
        else if (key == MENU_KEY_ENTER && stage == STAGE_DONE) {
            frame_depth = 1;
            screen_version += 1;
        }
        else if (key == MENU_KEY_ENTER && two && frame->softkey == 1) {
            frame->code_shown = true;
            frame->code_seen = access_point_mode_addresses_given();
        }
        else if (key == MENU_KEY_ENTER && stage == STAGE_CODE && frame->code_shown) {
            frame->code_shown = false;
        }
        else if (key == MENU_KEY_ENTER) {
            pop();
        }
        return;
    }
    if (page->kind == PAGE_DIALOG) {
        // The knob moves between Cancel and the action; a click does the focused one
        if (key == MENU_KEY_NEXT) {
            frame->softkey = 1;
        }
        else if (key == MENU_KEY_PREV) {
            frame->softkey = 0;
        }
        else if (key == MENU_KEY_ENTER && frame->softkey == 0) {
            pop();
        }
        else if (key == MENU_KEY_ENTER) {
            for (uint8_t idx = 0; idx < page->row_count; idx += 1) {
                if (page->rows[idx].kind == ROW_ACTION) {
                    page->rows[idx].act(&page->rows[idx]);
                    return;
                }
            }
        }
        return;
    }
    // As on a list: turned up past the top, the title is focused (focus -1), and a click there is Back
    if (frame->focus < 0) {
        if (key == MENU_KEY_NEXT) {
            frame->focus = 0;
        }
        else if (key == MENU_KEY_ENTER) {
            pop();
        }
        return;
    }
    uint8_t last = page_last_first(page);
    if (frame->at_end) {
        frame->first = last;
    }
    if (key == MENU_KEY_NEXT && frame->first < last) {
        frame->first += 1;
    }
    else if (key == MENU_KEY_PREV) {
        if (frame->first > 0) {
            frame->first -= 1;
        }
        else {
            frame->focus = -1;
        }
    }
    else if (key == MENU_KEY_ENTER && page->kind == PAGE_NOTES) {
        // At the end a click presses the button; before it, it turns the page, so the notes are read on the way
        if (frame->first >= last) {
            char lines[MENU_ROWS_MAX][MENU_TEXT];
            bool bolds[MENU_ROWS_MAX];
            const menu_row_t *button;
            page_lines(page, lines, nav_values_unused, bolds, &button);
            if (button != NULL) {
                button->act(button);
            }
        }
        else {
            frame->first = (uint8_t) (frame->first + nav_lines_shown < last ? frame->first + nav_lines_shown : last);
        }
    }
    frame->at_end = frame->focus >= 0 && frame->first >= last;
}


void menu_nav_key(menu_key_t key) {
    frame_t *frame = top();
    if (frame->kind == FRAME_PAGE && frame->page->kind != PAGE_LIST) {
        page_kind_key(frame, key);
        return;
    }
    switch (frame->kind) {
        case FRAME_EDIT:
            edit_key(frame, key);
            break;
        case FRAME_JOB:
            job_key(frame, key);
            break;
        case FRAME_MESSAGE:
            list_key(frame, key, 1);
            break;
        case FRAME_CHOICE:
            list_key(frame, key, choice_shown_count(frame->row));
            break;
        case FRAME_PAGE: {
            const menu_row_t *rows[MENU_ROWS_MAX];
            list_key(frame, key, shown_rows(frame->page, rows));
            break;
        }
    }
}


void menu_nav_init(void) {
    frame_depth = 0;
    push_page(&page_main);
}


// --- Jobs

void menu_nav_job_begun(activity_t job) {
    // What a person had open over a page goes: an editor, a list, a message, a start's acknowledgement
    while (frame_depth > 1 && (top()->kind != FRAME_PAGE || strncmp(top()->page->id, "ack/", 4) == 0)) {
        pop();
    }
    // And a number being set in its row is put back, nothing stored: the job runs on what was confirmed
    for (int idx = 0; idx < frame_depth; idx += 1) {
        inplace_cancel(&frames[idx]);
    }
    frame_t *frame = push(FRAME_JOB);
    frame->job = job;
    frame->softkey = 1;
    frame->action_seen = UINT8_MAX;         // none seen: the first is focused
    frame->focus = 0;                       // the cleanup: Speed
}


void menu_nav_job_ended(uint8_t exit_form) {
    int idx = frame_depth - 1;
    while (idx > 0 && frames[idx].kind != FRAME_JOB) {
        idx -= 1;
    }
    if (idx > 0) {
        frame_depth = idx;
        screen_version += 1;
    }
    if (exit_form == JOBS_EXIT_MOTOR_FAULT) {
        const char *fault = motors_get_fault();
        push_message(fault != NULL ? fault : "Motor drivers", "Charge and purge", "cannot start");
    }
}


void menu_nav_refresh(void) {
    chart_sample();
    flow_sample();
    // What was opened over the charge's screen between throws (its target, its powder, a message) is for the next
    // throw only: when a throw begins it closes, so the next Back reaches the charge and stops it (review of gate 4)
    int job_idx = -1;
    for (int idx = 0; idx < frame_depth; idx += 1) {
        if (frames[idx].kind == FRAME_JOB) {
            job_idx = idx;
        }
    }
    if (job_idx >= 0 && job_idx < frame_depth - 1 && activity_in_throw()) {
        frame_depth = job_idx + 1;
        screen_version += 1;
    }
    // The set-up: through it the screen shows its stage as it changes (the owner: two screens, one process), unless
    // a job holds the screen; on the hotspot, or at its end. Back leaves it until the next change
    static uint32_t setup_changes_seen = 0;
    uint32_t changes = setup_changes();
    frame_t *frame = top();
    bool on_setup = frame->kind == FRAME_PAGE && frame->page->kind == PAGE_CODE;
    if (changes != setup_changes_seen) {
        setup_changes_seen = changes;
        if (!on_setup && !menu_nav_in_job() && (wifi_is_access_point() || setup_step() == SETUP_STEP_DONE)) {
            menu_nav_goto("setup");
            frame = top();
            on_setup = true;
        }
    }
    if (!on_setup) {
        return;
    }
    // The code asked for, and a phone joined since: back to the stage
    if (frame->code_shown && access_point_mode_addresses_given() != frame->code_seen) {
        frame->code_shown = false;
    }
    setup_stage_t stage = setup_stage(frame);
    if (stage != frame->stage) {
        frame->stage = (uint8_t) stage;
        frame->softkey = stage == STAGE_CODE || stage == STAGE_DONE ? 1 : 0;
        screen_version += 1;
    }
}


bool menu_nav_in_job(void) {
    for (int idx = 0; idx < frame_depth; idx += 1) {
        if (frames[idx].kind == FRAME_JOB) {
            return true;
        }
    }
    return false;
}


// --- The console: pages by id

size_t menu_nav_page_count(void) {
    return ALL_PAGES_COUNT;
}


const char *menu_nav_page_id(size_t idx) {
    return idx < ALL_PAGES_COUNT ? all_pages[idx]->id : NULL;
}


// The path from the main menu to a page: its pages in order, the main menu first. False when it is not in the tree
static bool page_path(const menu_page_t *from, const menu_page_t *wanted, const menu_page_t **path, int *depth) {
    path[(*depth)++] = from;
    if (from == wanted) {
        return true;
    }
    for (uint8_t idx = 0; idx < from->row_count && *depth < MENU_FRAMES_MAX; idx += 1) {
        if (from->rows[idx].kind == ROW_PAGE && page_path(from->rows[idx].page, wanted, path, depth)) {
            return true;
        }
    }
    *depth -= 1;
    return false;
}


bool menu_nav_goto(const char *id) {
    const menu_page_t *wanted = NULL;
    for (size_t idx = 0; idx < ALL_PAGES_COUNT; idx += 1) {
        if (strcmp(all_pages[idx]->id, id) == 0) {
            wanted = all_pages[idx];
        }
    }
    if (wanted == NULL || menu_nav_in_job()) {
        return false;
    }
    const menu_page_t *path[MENU_FRAMES_MAX];
    int depth = 0;
    frame_depth = 0;
    if (wanted == &page_ack_charge || wanted == &page_ack_cleanup) {
        // A start's acknowledgement sits over the page it starts from
        page_path(&page_main, wanted == &page_ack_charge ? &page_charge : &page_main, path, &depth);
        path[depth++] = wanted;
    }
    else {
        page_path(&page_main, wanted, path, &depth);
    }
    for (int idx = 0; idx < depth; idx += 1) {
        push_page(path[idx]);
    }
    // Each parent focused on the row that leads on, as if the knob had opened it: Back lands there
    for (int idx = 0; idx + 1 < depth; idx += 1) {
        const menu_row_t *rows[MENU_ROWS_MAX];
        uint8_t count = shown_rows(path[idx], rows);
        for (uint8_t row = 0; row < count; row += 1) {
            if (rows[row]->kind == ROW_PAGE && rows[row]->page == path[idx + 1]) {
                frames[idx].focus = (int8_t) row;
            }
        }
    }
    screen_version += 1;
    return true;
}


// --- The screen, in words

static void screen_marks(menu_screen_t *screen) {
    wireless_info_t info;
    wireless_get_link(&info);
    screen->mark_station = strcmp(info.mode, "station") == 0 && info.address[0] != '\0';
    screen->mark_access_point = strcmp(info.mode, "access_point") == 0;
    activity_t job = activity_get();
    bool job_remote = (job == ACTIVITY_CHARGE || job == ACTIVITY_CLEANUP || job == ACTIVITY_UPDATE ||
                       job == ACTIVITY_CALIBRATION) &&
                      (activity_job_by() == ACTIVITY_BY_API || activity_job_by() == ACTIVITY_BY_CONSOLE);
    screen->mark_remote = job_remote || http_server_wrote_within(MENU_REMOTE_MS);
    screen->mark_simulated = scale_simulation_is_on();
}


static void screen_row(menu_screen_t *screen, const char *label, const char *value, bool opens) {
    if (screen->row_count == MENU_ROWS_MAX) {
        return;
    }
    snprintf(screen->row_label[screen->row_count], MENU_TEXT, "%s", label);
    snprintf(screen->row_value[screen->row_count], MENU_TEXT, "%s", value != NULL ? value : "");
    screen->row_opens[screen->row_count] = opens;
    screen->row_count += 1;
}


// A dialog: its info rows the message, its action the right softkey, Back the left
static void screen_dialog(menu_screen_t *screen, const menu_page_t *page, const frame_t *frame) {
    screen->kind = MENU_SCREEN_DIALOG;
    for (uint8_t idx = 0; idx < page->row_count; idx += 1) {
        const menu_row_t *row = &page->rows[idx];
        if (row->kind == ROW_ACTION) {
            snprintf(screen->softkey_right, MENU_TEXT, "%s", row->label);
        }
        else {
            screen_row(screen, row->label, "", false);
        }
    }
    snprintf(screen->softkey_left, MENU_TEXT, "Cancel");
    screen->softkey_focus = frame->softkey;
}


static void setup_lines(menu_screen_t *screen, const char *title, const char *a, const char *b, const char *c) {
    screen->kind = MENU_SCREEN_DIALOG;
    snprintf(screen->title, sizeof(screen->title), "%s", title);
    screen_row(screen, a, "", false);
    if (b != NULL) {
        screen_row(screen, b, "", false);
    }
    if (c != NULL) {
        screen_row(screen, c, "", false);
    }
}


// A driver's state in a line: "Coarse driver: found", or what is wrong with it
static void setup_driver_line(char *out, size_t room, const char *name, motor_select_t motor) {
    const char *state = motor_driver_get_status_string(motor);
    snprintf(out, room, "%s driver: %s", name, state == NULL || strcmp(state, "absent") == 0 ? "not found" :
                                               strcmp(state, "ok") == 0 ? "found" : state);
}


static void screen_setup(menu_screen_t *screen, const menu_page_t *page, const frame_t *frame) {
    setup_stage_t stage = setup_stage(frame);
    wireless_station_info_t station;
    wireless_get_station(&station);
    char network[SETUP_NETWORK_MAX];
    setup_network(network, sizeof(network));
    const char *name = network[0] != '\0' ? network : station.ssid;
    char line[MENU_TEXT];
    char other[MENU_TEXT];
    screen->softkey_focus = frame->softkey;
    switch (stage) {
        case STAGE_CODE:
            screen->kind = MENU_SCREEN_CODE;
            for (uint8_t idx = 0; idx < page->row_count; idx += 1) {
                screen_row(screen, page->rows[idx].label, "", false);
            }
            setup_code_text(screen->code, sizeof(screen->code));
            snprintf(screen->softkey_right, MENU_TEXT, "Back");
            screen->softkey_focus = 1;
            return;
        case STAGE_CHOOSE:
            setup_lines(screen, "Set up: Wi-Fi", "A phone joined.", "Choose your Wi-Fi", "on your phone.");
            snprintf(screen->softkey_right, MENU_TEXT, "Show the code");
            break;
        case STAGE_PASSWORD:
            setup_lines(screen, "Set up: Wi-Fi", "Type the password of", name, "on your phone.");
            snprintf(screen->softkey_right, MENU_TEXT, "Show the code");
            break;
        case STAGE_JOINING:
            // Its seconds counting: a still screen reads as stuck (the owner)
            snprintf(line, sizeof(line), "%lu s of %lu", (unsigned long) (station.joining_ms / 1000),
                     (unsigned long) (wireless_join_timeout_ms() / 1000));
            setup_lines(screen, "Set up: Wi-Fi", "Joining", station.ssid, line);
            break;
        case STAGE_JOINED:
            setup_lines(screen, "Set up: Wi-Fi", "Joined", station.ssid, "Continue on your phone.");
            break;
        case STAGE_WRONG_PASSWORD:
            setup_lines(screen, "Set up: Wi-Fi", "Wrong password for", name, "Retype it on your phone.");
            break;
        case STAGE_NO_ANSWER:
            setup_lines(screen, "Set up: Wi-Fi", "No answer from", name, "Move it nearer the router.");
            break;
        case STAGE_NOT_JOINED:
            setup_lines(screen, "Set up: Wi-Fi", "Not joined to", name, "See your phone.");
            break;
        case STAGE_SCALE: {
            screen->kind = MENU_SCREEN_JOB;
            snprintf(screen->title, sizeof(screen->title), "Set up: Scale");
            float weight = scale_get_current_measurement();
            if (isfinite(weight)) {
                format_weight(weight, screen->figure, sizeof(screen->figure));
                snprintf(screen->unit, sizeof(screen->unit), "%s", unit_text());
                snprintf(screen->context, sizeof(screen->context), "The scale sends its weight");
            }
            else {
                snprintf(screen->figure, sizeof(screen->figure), "--");
                snprintf(screen->context, sizeof(screen->context), "Nothing from the scale");
            }
            screen->edit_at = -1;
            break;
        }
        case STAGE_MOTORS:
            setup_driver_line(line, sizeof(line), "Coarse", SELECT_COARSE_TRICKLER_MOTOR);
            setup_driver_line(other, sizeof(other), "Fine", SELECT_FINE_TRICKLER_MOTOR);
            setup_lines(screen, "Set up: Motors", line, other, "Continue on your phone.");
            break;
        case STAGE_READY:
            setup_lines(screen, "Set up: Ready", "Ready.", "Finish on your phone.", NULL);
            break;
        case STAGE_DONE: {
            wireless_info_t info;
            wireless_get_link(&info);
            if (strcmp(station.state, "joined") == 0) {
                snprintf(line, sizeof(line), "Open on %s:", station.ssid);
                snprintf(other, sizeof(other), "%s.local", info.hostname);
                char address[MENU_TEXT];
                snprintf(address, sizeof(address), "or %s", station.address);
                setup_lines(screen, "Set up: Done", line, other, address);
            }
            else {
                setup_lines(screen, "Set up: Done", "On its own hotspot.", "Open " ACCESS_POINT_MODE_ADDRESS,
                            "from a phone on it.");
            }
            snprintf(screen->softkey_right, MENU_TEXT, "OK");
            screen->softkey_focus = 1;
            return;
        }
    }
    snprintf(screen->softkey_left, MENU_TEXT, "Back");
}


// Notes: their lines from the first shown, ending in their button
static void screen_lines(menu_screen_t *screen, const frame_t *frame) {
    const menu_page_t *page = frame->page;
    screen->kind = MENU_SCREEN_NOTES;
    const menu_row_t *button;
    screen->row_count = page_lines(page, screen->row_label, screen->row_value, screen->row_bold, &button);
    uint8_t last = page_last_first(page);
    screen->first = frame->at_end || frame->first > last ? last : frame->first;
    screen->focus = frame->focus < 0 ? -1 : 0;
    if (button != NULL) {
        snprintf(screen->button, MENU_TEXT, "%s", button->label_now != NULL ? button->label_now() : button->label);
        screen->button_reached = screen->first >= last && screen->focus >= 0;
    }
}


static void screen_page(menu_screen_t *screen, const frame_t *frame) {
    const menu_page_t *page = frame->page;
    snprintf(screen->id, sizeof(screen->id), "%s", page->id);
    snprintf(screen->title, sizeof(screen->title), "%s", page->title_now != NULL ? page->title_now() : page->title);
    screen->title_back = frame_depth > 1 && page->kind != PAGE_DIALOG && page->kind != PAGE_CODE;
    if (page->kind == PAGE_DIALOG) {
        screen_dialog(screen, page, frame);
        return;
    }
    if (page->kind == PAGE_CODE) {
        screen_setup(screen, page, frame);
        return;
    }
    if (page->kind != PAGE_LIST) {
        screen_lines(screen, frame);
        return;
    }
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t count = shown_rows(page, rows);
    for (uint8_t idx = 0; idx < count; idx += 1) {
        const menu_row_t *row = rows[idx];
        char value[MENU_TEXT] = "";
        if (row->value != NULL) {
            row->value(row, value, sizeof(value));
        }
        bool opens = row->kind == ROW_PAGE || row->kind == ROW_CHOICE || row->kind == ROW_NUMBER;
        screen->row_info[screen->row_count] = row->kind == ROW_INFO;
        screen_row(screen, row_label(row), value, opens);
    }
    screen->focus = frame->focus < count ? frame->focus : (int8_t) (count - 1);
    screen->editing = frame->editing;
}


// A choice's page: its options, the current one marked
static void screen_choice(menu_screen_t *screen, const frame_t *frame) {
    const menu_row_t *row = frame->row;
    snprintf(screen->id, sizeof(screen->id), "choice");
    snprintf(screen->title, sizeof(screen->title), "%s", row_label(row));
    screen->title_back = true;
    int now = row_choice_now(row);
    for (int idx = 0; idx < row_choice_count(row); idx += 1) {
        const char *label = row_choice_label(row, idx);
        if (label != NULL) {
            screen->row_chosen[screen->row_count] = idx == now;
            screen_row(screen, label, "", false);
        }
    }
    screen->focus = frame->focus;
}


// The scale's unit as a person reads it: "gr" for grains, as the powder profiles write it; "" when not known
static const char *unit_text(void) {
    const char *unit = scale_get_unit();
    if (unit == NULL) {
        return "";
    }
    return strcmp(unit, "gn") == 0 ? "gr" : unit;
}


static void screen_edit(menu_screen_t *screen, const frame_t *frame) {
    screen->kind = MENU_SCREEN_EDIT;
    snprintf(screen->id, sizeof(screen->id), "edit");
    snprintf(screen->title, sizeof(screen->title), "%s", frame->row->label);
    // The digits with their point: 024.35 or 24.350; the one focused (none while a softkey is), and whether it is
    // being set
    size_t len = 0;
    int whole = MENU_EDIT_DIGITS - frame->decimals;
    screen->edit_at = -1;
    screen->editing = frame->editing;
    for (int idx = 0; idx < MENU_EDIT_DIGITS; idx += 1) {
        if (idx == whole) {
            screen->figure[len++] = '.';
        }
        if (idx == frame->digit_at) {
            screen->edit_at = (int8_t) len;
        }
        screen->figure[len++] = (char) ('0' + frame->digits[idx]);
    }
    screen->figure[len] = '\0';
    snprintf(screen->unit, sizeof(screen->unit), "%s", unit_text());
    // Past the digits the focus is on Cancel, then Set
    snprintf(screen->softkey_left, MENU_TEXT, "Cancel");
    snprintf(screen->softkey_right, MENU_TEXT, "Set");
    screen->softkey_focus = (int8_t) (frame->digit_at >= MENU_EDIT_DIGITS ? frame->digit_at - MENU_EDIT_DIGITS : -1);
}


// A message, as a dialog: a click or Back closes it
static void screen_message(menu_screen_t *screen, const frame_t *frame) {
    screen->kind = MENU_SCREEN_DIALOG;
    snprintf(screen->id, sizeof(screen->id), "message");
    snprintf(screen->title, sizeof(screen->title), "%s", frame->message[0]);
    screen_row(screen, frame->message[1], "", false);
    if (frame->message[2][0] != '\0') {
        screen_row(screen, frame->message[2], "", false);
    }
    snprintf(screen->softkey_right, MENU_TEXT, "OK");
    screen->softkey_focus = 1;
}


static const char *charge_state_text(const char *state) {
    if (strcmp(state, "zeroing") == 0) {
        return "Zeroing";
    }
    if (strcmp(state, "charging") == 0) {
        return "Charging";
    }
    if (strcmp(state, "waiting_for_cup_removal") == 0) {
        return "Lift the cup";
    }
    if (strcmp(state, "waiting_for_cup_return") == 0) {
        return "Return the cup";
    }
    if (strcmp(state, "starting") == 0) {
        return "Starting";
    }
    return state;
}


// The title a part's own screens have: the cup's, the scale's, or the powder's name
static const char *calibration_part_title(const calibration_status_t *cal) {
    if (cal->part == CALIBRATION_PART_POWDER && cal->profile >= 0) {
        return profile_get_name((uint16_t) cal->profile);
    }
    return cal->part == CALIBRATION_PART_SCALE ? "Scale" : "Cup";
}


static void calibration_dialog(menu_screen_t *screen, const char *title, const char *line1, const char *line2,
                               const char *line3) {
    screen->kind = MENU_SCREEN_DIALOG;
    snprintf(screen->title, sizeof(screen->title), "%s", title);
    screen_row(screen, line1, "", false);
    if (line2 != NULL) {
        screen_row(screen, line2, "", false);
    }
    if (line3 != NULL) {
        screen_row(screen, line3, "", false);
    }
}


static void calibration_weight_figure(menu_screen_t *screen, float weight) {
    format_weight(weight, screen->figure, sizeof(screen->figure));
    snprintf(screen->unit, sizeof(screen->unit), "%s", unit_text());
}


static void screen_calibration(menu_screen_t *screen, frame_t *frame) {
    calibration_status_t cal;
    calibration_get_status(&cal);
    calibration_step_seen(frame, &cal);
    snprintf(screen->id, sizeof(screen->id), "job/calibration");
    char line[MENU_TEXT];
    char weight[16];
    const char *unit = unit_text();
    const char *space = unit[0] != '\0' ? " " : "";
    switch (cal.step) {
        case CALIBRATION_STEP_CUP_ON:
            if (cal.part == CALIBRATION_PART_POWDER) {
                // The powder calibrated is the one in the hopper
                snprintf(line, sizeof(line), "%s in the hopper", calibration_part_title(&cal));
                calibration_dialog(screen, calibration_part_title(&cal), line, "Put the empty cup", "on the scale");
            }
            else {
                calibration_dialog(screen, calibration_part_title(&cal), "Put the empty cup", "on the scale", NULL);
            }
            break;
        case CALIBRATION_STEP_ZEROING:
            snprintf(screen->title, sizeof(screen->title), "Zeroing");
            calibration_weight_figure(screen, cal.weight);
            break;
        case CALIBRATION_STEP_LIFT:
            snprintf(screen->title, sizeof(screen->title), "Lift the cup");
            calibration_weight_figure(screen, cal.weight);
            snprintf(screen->context, MENU_TEXT, "Its weight is read");
            break;
        case CALIBRATION_STEP_BACK:
            snprintf(screen->title, sizeof(screen->title), "Put the cup back");
            calibration_weight_figure(screen, cal.weight);
            format_weight(cal.cup_weight, weight, sizeof(weight));
            snprintf(screen->context, MENU_TEXT, "The cup weighs %s%s%s", weight, space, unit);
            break;
        case CALIBRATION_STEP_FILL:
            // A job whose turn moves the coarse tube: the powder in the cup as the figure, what the knob does under
            // it; Full is the click, drawn focused; no Cancel softkey the knob could not reach: Back cancels, the
            // title's "<" says so (the owner, 2026-11-25)
            snprintf(screen->title, sizeof(screen->title), "Fill the cup");
            screen->title_back = true;
            calibration_weight_figure(screen, cal.weight);
            snprintf(screen->context, MENU_TEXT, "Rotate to dispense");
            screen->softkey_left[0] = '\0';
            snprintf(screen->softkey_right, MENU_TEXT, "It's full");
            screen->softkey_focus = 1;
            return;
        case CALIBRATION_STEP_KEEP:
            format_weight(cal.capacity, weight, sizeof(weight));
            snprintf(line, sizeof(line), "It holds %s%s%s", weight, space, unit);
            calibration_dialog(screen, "Cup", line, "Keep it as its capacity?", NULL);
            break;
        case CALIBRATION_STEP_EMPTY:
            calibration_dialog(screen, "Empty the cup", "Then put it back", "on the scale", NULL);
            break;
        case CALIBRATION_STEP_RUNNING:
            snprintf(screen->title, sizeof(screen->title), "Calibrating");
            snprintf(screen->figure, sizeof(screen->figure), "%d", cal.progress);
            snprintf(screen->unit, sizeof(screen->unit), "%%");
            snprintf(screen->context, MENU_TEXT, "%s", cal.line);
            break;
        case CALIBRATION_STEP_PAUSED:
            snprintf(line, sizeof(line), "%s%s%s", cal.line, space, unit);
            calibration_dialog(screen, "Empty the cup", line, "Then go on", NULL);
            break;
        case CALIBRATION_STEP_RESULT: {
            screen->kind = MENU_SCREEN_NOTES;
            snprintf(screen->title, sizeof(screen->title), "%s", calibration_part_title(&cal));
            screen->title_back = true;
            screen->row_count = calibration_result_lines(&cal, screen->row_label, screen->row_value, screen->row_bold);
            uint8_t last = calibration_last_first(&cal);
            screen->first = frame->first > last ? last : frame->first;
            screen->focus = 0;
            snprintf(screen->button, MENU_TEXT, "%s", cal.part == CALIBRATION_PART_SCALE ? "Keep" : "Save");
            screen->button_reached = screen->first >= last;
            screen->softkey_left[0] = '\0';
            return;
        }
        case CALIBRATION_STEP_FAILED:
            // Said as a failure, never as something kept (the owner, 2026-11-25: "Its records are kept" read as done)
            calibration_dialog(screen, "Not calibrated", cal.failure, "Nothing changed", NULL);
            break;
        default:
            snprintf(screen->title, sizeof(screen->title), "Calibration");
            break;
    }
    const char *label = calibration_answer_label(&cal, NULL);
    snprintf(screen->softkey_left, MENU_TEXT, "%s", calibration_left_label(cal.step));
    snprintf(screen->softkey_right, MENU_TEXT, "%s", label != NULL ? label : "");
    screen->softkey_focus = frame->softkey;
}

/*
 * A job's screen (the skill, rule 5): its status as the title, one figure and its unit, one line under it, two
 * softkeys. Who started it shows as the remote mark, not in words
 */
static void screen_job(menu_screen_t *screen, frame_t *frame) {
    screen->kind = MENU_SCREEN_JOB;
    screen->edit_at = -1;
    snprintf(screen->softkey_left, MENU_TEXT, "Stop");

    if (frame->job == ACTIVITY_CALIBRATION) {
        screen_calibration(screen, frame);
        return;
    }

    if (frame->job == ACTIVITY_CHARGE) {
        charge_status_t charge;
        charge_mode_get_status(&charge);
        snprintf(screen->id, sizeof(screen->id), "job/charge");
        bool fault = strcmp(charge.state, "fault") == 0;
        if (fault) {
            snprintf(screen->title, sizeof(screen->title), "%s", charge_mode_get_title());
        }
        else {
            snprintf(screen->title, sizeof(screen->title), "%s", charge.paused ? "Paused" :
                                                                  charge_state_text(charge.state));
            // The time, as the old screen had it: live while a throw pours, the last throw's while it waits
            if (charge.elapsed_s > 0.0f) {
                snprintf(screen->title_note, sizeof(screen->title_note), "%.2f s", (double) charge.elapsed_s);
            }
        }
        // Between throws the throw's weight stays, with its verdict: with the cup off, the pan's reading means
        // nothing to a person
        bool between = strcmp(charge.state, "waiting_for_cup_removal") == 0 ||
                       strcmp(charge.state, "waiting_for_cup_return") == 0;
        // Only this throw's verdict: until it is read (the scale's lag and settle time after the stop, ADR-009), the
        // last one is the throw before's, and the live reading is shown
        bool verdict = between && charge.last_throw > 0 && charge.last_throw == charge.throw_number &&
                       charge.last_result != EVENT_RESULT_NONE;
        format_weight(verdict ? charge.last_weight : scale_get_current_measurement(), screen->figure,
                      sizeof(screen->figure));
        snprintf(screen->unit, sizeof(screen->unit), "%s", unit_text());
        if (verdict) {
            char off[16];
            format_weight(fabsf(charge.last_weight - charge.target), off, sizeof(off));
            if (charge.last_result == EVENT_RESULT_GOOD) {
                snprintf(screen->context, MENU_TEXT, "Good");
            }
            else {
                snprintf(screen->context, MENU_TEXT, "%s by %s",
                         charge.last_result == EVENT_RESULT_OVER ? "Over" : "Under", off);
            }
            screen->context_bold = true;
        }
        else if (!fault && charge.target > 0.0f) {
            char target[16];
            format_weight(charge.target, target, sizeof(target));
            snprintf(screen->context, MENU_TEXT, "of %s, %s", target, profile_get_name(profile_get_selected_idx()));
        }
        // Charge view on Chart: the weight over the target in its head, the powder (or the verdict) beside it
        if (!fault && charge.target > 0.0f && chart_chosen()) {
            char target[16];
            format_weight(charge.target, target, sizeof(target));
            screen->chart = true;
            snprintf(screen->chart_head, sizeof(screen->chart_head), "%s / %s", screen->figure, target);
            if (!verdict) {
                snprintf(screen->context, MENU_TEXT, "%s", profile_get_name(profile_get_selected_idx()));
            }
            screen->chart_count = chart_count;
            screen->chart_step_ms = chart_step_ms;
            memcpy(screen->chart_points, chart_points, chart_count);
        }
        // A new action is focused as it appears; with none, Stop is
        job_action_t action = charge_action();
        if (action != frame->action_seen) {
            frame->action_seen = (uint8_t) action;
            frame->softkey = action != JOB_ACTION_NONE ? 1 : 0;
        }
        snprintf(screen->softkey_right, MENU_TEXT, "%s", job_action_name(action));
        screen->softkey_focus = frame->softkey;
        return;
    }

    if (frame->job == ACTIVITY_CLEANUP) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        screen->kind = MENU_SCREEN_CONTROL;
        snprintf(screen->id, sizeof(screen->id), "job/purge");
        bool fault = strcmp(cleanup.state, "fault") == 0;
        bool running = cleanup.speed_rps != 0.0f;
        snprintf(screen->title, sizeof(screen->title), "%s", fault ? cleanup_mode_get_title() :
                                                             (running ? "Purge: running" : "Purge"));
        // The weight and the flow to read; Speed and Gate set at the knob
        char weight[16];
        char text[MENU_TEXT];
        format_weight(scale_get_current_measurement(), weight, sizeof(weight));
        snprintf(text, sizeof(text), "%s%s%s", weight, unit_text()[0] != '\0' ? " " : "", unit_text());
        char flow[MENU_TEXT] = "";
        if (isfinite(flow_per_s)) {
            snprintf(flow, sizeof(flow), "%.2f%s%s/s", (double) flow_per_s, unit_text()[0] != '\0' ? " " : "",
                     unit_text());
        }
        screen->row_info[0] = true;
        screen_row(screen, text, flow, false);
        snprintf(text, sizeof(text), "%.1f rev/s", (double) cleanup.speed_rps);
        screen_row(screen, "Speed", text, false);
        screen_row(screen, "Gate", servo_gate_state() == GATE_OPEN ? "Open" :
                                   (servo_gate_state() == GATE_CLOSE ? "Closed" : "Off"), false);
        if (fault) {
            snprintf(screen->softkey_right, MENU_TEXT, "OK");
            screen->softkey_focus = frame->softkey;
            screen->focus = 3;
            return;
        }
        snprintf(screen->softkey_right, MENU_TEXT, "Zero");
        // The focus: -1 the title, 1 and 2 the rows (row 0 is the line to read), 3 the softkeys
        screen->title_back = true;
        screen->focus = (int8_t) (frame->focus == CLEANUP_TITLE ? -1 :
                                  (frame->focus <= CLEANUP_GATE ? frame->focus + 1 : 3));
        screen->softkey_focus = (int8_t) (frame->focus == CLEANUP_STOP ? 0 : (frame->focus == CLEANUP_ZERO ? 1 : -1));
        screen->editing = frame->editing;
        return;
    }

    update_status_t update;
    update_status(&update);
    snprintf(screen->id, sizeof(screen->id), "job/update");
    update_poll_status_t poll;
    bool found = update.download && update_found(&poll);
    switch (update.state) {
        case UPDATE_RECEIVING:
            if (update.expected == 0) {
                snprintf(screen->title, sizeof(screen->title), "%s", update.download ? "Connecting" :
                                                                     "Waiting for the image");
            }
            else if (found) {
                snprintf(screen->title, sizeof(screen->title), "Downloading %.15s", poll.version);
            }
            else {
                snprintf(screen->title, sizeof(screen->title), "%s", update.download ? "Downloading" : "Receiving");
            }
            break;
        case UPDATE_CHECKING:
            snprintf(screen->title, sizeof(screen->title), "Checking");
            break;
        case UPDATE_READY:
            snprintf(screen->title, sizeof(screen->title), "%s", update.download ? "Checking" : "Ready to install");
            break;
        case UPDATE_INSTALLING:
            snprintf(screen->title, sizeof(screen->title), "Installing");
            break;
        default:
            snprintf(screen->title, sizeof(screen->title), "Waiting for the image");
            break;
    }
    if (update.expected > 0) {
        snprintf(screen->figure, sizeof(screen->figure), "%lu",
                 (unsigned long) ((uint64_t) update.received * 100u / update.expected));
        snprintf(screen->unit, sizeof(screen->unit), "%%");
    }
    snprintf(screen->context, MENU_TEXT, "Keep the power on");
    // Stop, its one softkey, focused, until it installs
    bool stoppable = update.state != UPDATE_INSTALLING;
    snprintf(screen->softkey_left, MENU_TEXT, "%s", stoppable ? "Stop" : "");
    screen->softkey_focus = (int8_t) (stoppable ? 0 : -1);
}


void menu_nav_screen(menu_screen_t *screen) {
    memset(screen, 0, sizeof(*screen));
    screen->kind = MENU_SCREEN_LIST;
    screen->focus = -1;
    screen->softkey_focus = -1;
    screen->version = screen_version;
    frame_t *frame = top();
    switch (frame->kind) {
        case FRAME_PAGE:
            screen_page(screen, frame);
            break;
        case FRAME_CHOICE:
            screen_choice(screen, frame);
            break;
        case FRAME_EDIT:
            screen_edit(screen, frame);
            break;
        case FRAME_MESSAGE:
            screen_message(screen, frame);
            break;
        case FRAME_JOB:
            screen_job(screen, frame);
            break;
    }
    screen_marks(screen);
}
