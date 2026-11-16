/*
    The menu's navigator (ADR-010, gate 4): the tree of menu_design.md as one table, what is open, and the keys.
    Every write goes through the entry point the API uses for the same thing, by button; a setting is stored when it
    is confirmed (D2). No row draws: menu_nav_screen() says what is on screen, in words.
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
#include "cleanup_mode.h"
#include "common.h"
#include "eeprom.h"
#include "http_server.h"
#include "jobs.h"
#include "menu_nav.h"
#include "motors.h"
#include "profile.h"
#include "scale.h"
#include "servo_gate.h"
#include "settings.h"
#include "system_control.h"
#include "update.h"
#include "version.h"
#include "wireless.h"

// A write to the API this recent counts as the unit being controlled from elsewhere (menu_design.md, proposed)
#define MENU_REMOTE_MS 30000

// The gate's state, for the cleanup's screen (servo_gate.c)
extern servo_gate_t servo_gate;
#define MENU_FRAMES_MAX 8
#define MENU_EDIT_DIGITS 5

typedef struct menu_row menu_row_t;
typedef struct menu_page menu_page_t;

typedef enum {
    ROW_PAGE,       // opens a page
    ROW_ACTION,     // does something
    ROW_CHOICE,     // one of a list, which it opens
    ROW_NUMBER,     // a number, which the editor sets
    ROW_TOGGLE,     // on or off, flipped by a click
    ROW_INFO,       // read only
} row_kind_t;

struct menu_row {
    const char *label;
    row_kind_t kind;
    const menu_page_t *page;                                                // ROW_PAGE
    void (*value)(const menu_row_t *row, char *out, size_t room);           // the value at the right; NULL: none
    const char *(*label_now)(void);                                         // a label that changes; NULL: label
    bool (*shown)(void);                                                    // NULL: always
    void (*act)(const menu_row_t *row);                                     // ROW_ACTION, ROW_TOGGLE
    const char *path;                                                       // a setting: its resource
    const char *name;                                                       // and its field
    bool (*offered)(const setting_choice_t *choice);                        // a setting's choice offered; NULL: all
    // A choice that is not a setting's (the powder profiles)
    int (*count)(void);
    const char *(*option)(int idx);
    int (*current)(void);
    const char *(*choose)(int idx);                                         // NULL when done, else why not
};

struct menu_page {
    const char *id;
    const char *title;
    const char *(*title_now)(void);                                         // a title that changes; NULL: title
    const menu_row_t *rows;
    uint8_t row_count;
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
    int8_t job_action;
} frame_t;

static frame_t frames[MENU_FRAMES_MAX];
static int frame_depth = 0;
static uint32_t screen_version = 1;


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
}


static void push_message(const char *title, const char *line1, const char *line2) {
    frame_t *frame = push(FRAME_MESSAGE);
    snprintf(frame->message[0], MENU_TEXT, "%s", title);
    snprintf(frame->message[1], MENU_TEXT, "%s", line1 != NULL ? line1 : "");
    snprintf(frame->message[2], MENU_TEXT, "%s", line2 != NULL ? line2 : "");
}


// --- A setting's choices and toggles: read and written through settings.c, the API's own checks

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
    return field->choices[idx].label;
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


static void value_board(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    snprintf(out, room, "%s", api_v1_board_name());
}


static void value_unit_id(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    char id[9];
    eeprom_get_board_id(id, 8);
    id[8] = '\0';
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


static bool wifi_is_access_point(void) {
    wireless_info_t info;
    wireless_get_link(&info);
    return strcmp(info.mode, "access_point") == 0;
}


static void value_wifi_mode(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    if (strcmp(info.mode, "access_point") == 0) {
        snprintf(out, room, "own access point");
    }
    else if (strcmp(info.mode, "station") == 0) {
        snprintf(out, room, "%s", info.address[0] != '\0' ? "joined" : "joining");
    }
    else {
        snprintf(out, room, "none");
    }
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


static void value_wifi_name(const menu_row_t *row, char *out, size_t room) {
    (void) row;
    wireless_info_t info;
    wireless_get_link(&info);
    snprintf(out, room, "%s", info.hostname != NULL ? info.hostname : "");
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


static void act_back(const menu_row_t *row) {
    (void) row;
    pop();
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


static const menu_page_t page_ack_charge;
static const menu_page_t page_ack_cleanup;
static const menu_row_t rows_charge[3];
static void push_edit(const menu_row_t *row);

// The charge page's Target and Powder, which the charge's own screen opens too between throws
#define ROW_TARGET (&rows_charge[1])
#define ROW_POWDER (&rows_charge[2])


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
    if (!motors_fault_shown("Cleanup")) {
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
        push_message(motors_get_fault(), "Cleanup", "cannot start");
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
    if (!profile_data_is_saved() && !profile_data_save()) {
        return "the EEPROM did not take it";
    }
    return NULL;
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


// 0 and 180 degrees: at 90 or 270 the 12864 is 64 pixels wide (menu_design.md, item 11)
static bool offered_rotation(const setting_choice_t *choice) {
    return choice->held == 0 || choice->held == 2;
}


static void value_setting_choice(const menu_row_t *row, char *out, size_t room) {
    int idx = setting_choice_now(row);
    const setting_field_t *field = settings_field(row->path, row->name);
    snprintf(out, room, "%s", field != NULL && idx >= 0 ? field->choices[idx].label : "?");
}


static void value_setting_toggle(const menu_row_t *row, char *out, size_t room) {
    snprintf(out, room, "%s", setting_is_on(row) ? "On" : "Off");
}


// --- The tree (menu_design.md)

static const menu_row_t rows_ack_charge[] = {
    {.label = "Put the cup on the scale", .kind = ROW_INFO},
    {.label = "Cup is on: start", .kind = ROW_ACTION, .act = act_ack_charge},
};
static const menu_page_t page_ack_charge = {.id = "ack/charge", .title = "Charge", .rows = rows_ack_charge,
                                            .row_count = 2};

static const menu_row_t rows_ack_cleanup[] = {
    {.label = "Put the cup on the scale", .kind = ROW_INFO},
    {.label = "Cup is on: start", .kind = ROW_ACTION, .act = act_ack_cleanup},
};
static const menu_page_t page_ack_cleanup = {.id = "ack/cleanup", .title = "Cleanup", .rows = rows_ack_cleanup,
                                             .row_count = 2};

static const menu_row_t rows_charge[3] = {
    {.label = "Start", .kind = ROW_ACTION, .act = act_charge_start},
    {.label = "Target", .kind = ROW_NUMBER, .value = value_target},
    {.label = "Powder", .kind = ROW_CHOICE, .value = value_powder, .count = powder_count, .option = powder_option,
     .current = powder_current, .choose = powder_choose},
};
static const menu_page_t page_charge = {.id = "charge", .title = "Charge", .rows = rows_charge, .row_count = 3};

static const menu_row_t rows_wifi[] = {
    {.label = "Wi-Fi", .kind = ROW_INFO, .value = value_wifi_mode},
    {.label = "Network", .kind = ROW_INFO, .value = value_wifi_network},
    {.label = "Key", .kind = ROW_INFO, .value = value_wifi_key, .shown = wifi_is_access_point},
    {.label = "Address", .kind = ROW_INFO, .value = value_wifi_address},
    {.label = "Name", .kind = ROW_INFO, .value = value_wifi_name},
    {.label = "Signal", .kind = ROW_INFO, .value = value_wifi_signal, .shown = wifi_is_station},
};
static const menu_page_t page_wifi = {.id = "wifi", .title = "Wi-Fi", .rows = rows_wifi, .row_count = 6};

static const menu_row_t rows_scale[] = {
    {.label = "Reading", .kind = ROW_INFO, .value = value_reading},
    {.label = "Zero", .kind = ROW_ACTION, .act = act_zero},
    {.label = "Make", .kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/scale", .name = "driver",
     .offered = offered_make},
    {.label = "Baud rate", .kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/scale",
     .name = "baud"},
    {.label = "Decimals", .kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/charge",
     .name = "decimal_places"},
    {.label = "Restart to apply", .kind = ROW_ACTION, .act = act_restart, .shown = shown_restart_pending},
};
static const menu_page_t page_scale = {.id = "settings/scale", .title = "Scale", .rows = rows_scale, .row_count = 6};

static const menu_row_t rows_display[] = {
    {.label = "Invert knob", .kind = ROW_TOGGLE, .value = value_setting_toggle, .act = act_toggle,
     .path = "settings/display", .name = "invert_encoder"},
    {.label = "Rotation", .kind = ROW_CHOICE, .value = value_setting_choice, .path = "settings/display",
     .name = "rotation", .offered = offered_rotation},
    {.label = "Restart to apply", .kind = ROW_ACTION, .act = act_restart, .shown = shown_restart_pending},
};
static const menu_page_t page_display = {.id = "settings/display", .title = "Display", .rows = rows_display,
                                         .row_count = 3};

static const menu_row_t rows_firmware[] = {
    {.label = "Version", .kind = ROW_INFO, .value = value_version},
    {.label = "Last update", .kind = ROW_INFO, .value = value_last_update, .shown = shown_last_update},
    {.label = "Board", .kind = ROW_INFO, .value = value_board},
    {.label = "Unit", .kind = ROW_INFO, .value = value_unit_id},
    {.label = "Build", .kind = ROW_INFO, .value = value_build},
};
static const menu_page_t page_firmware = {.id = "settings/firmware", .title = "Firmware", .rows = rows_firmware,
                                          .row_count = 5};

static const menu_row_t rows_factory_reset[] = {
    {.label = "Erases every setting", .kind = ROW_INFO},
    {.label = "Cancel", .kind = ROW_ACTION, .act = act_back},
    {.label = "Erase", .kind = ROW_ACTION, .act = act_erase},
};
static const menu_page_t page_factory_reset = {.id = "settings/factory_reset", .title = "Factory reset",
                                               .rows = rows_factory_reset, .row_count = 3};

static const menu_row_t rows_settings[] = {
    {.label = "Scale", .kind = ROW_PAGE, .page = &page_scale},
    {.label = "Display", .kind = ROW_PAGE, .page = &page_display},
    {.label = "Firmware", .kind = ROW_PAGE, .page = &page_firmware},
    {.label = "Restart", .kind = ROW_ACTION, .act = act_restart, .label_now = restart_label},
    {.label = "Factory reset", .kind = ROW_PAGE, .page = &page_factory_reset},
};
static const menu_page_t page_settings = {.id = "settings", .title = "Settings", .rows = rows_settings,
                                          .row_count = 5};

// "OpenTrickler", or the motor driver that is not OK (ADR-012, step 6)
static const char *main_title(void) {
    const char *fault = motors_get_fault();
    return fault != NULL ? fault : "OpenTrickler";
}

static const menu_row_t rows_main[] = {
    {.label = "Charge", .kind = ROW_PAGE, .page = &page_charge},
    {.label = "Cleanup", .kind = ROW_ACTION, .act = act_cleanup_start},
    {.label = "Wi-Fi", .kind = ROW_PAGE, .page = &page_wifi},
    {.label = "Settings", .kind = ROW_PAGE, .page = &page_settings},
};
static const menu_page_t page_main = {.id = "main", .title = "OpenTrickler", .title_now = main_title,
                                      .rows = rows_main, .row_count = 4};

// Every page, for the console and the menu's tour; a page's parents are found through the tree
static const menu_page_t *const all_pages[] = {
    &page_main, &page_charge, &page_wifi, &page_settings, &page_scale, &page_display, &page_firmware,
    &page_factory_reset, &page_ack_charge, &page_ack_cleanup,
};
#define ALL_PAGES_COUNT (sizeof(all_pages) / sizeof(all_pages[0]))


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


static void edit_key(frame_t *frame, menu_key_t key) {
    switch (key) {
        case MENU_KEY_NEXT:
            frame->digits[frame->digit_at] = (uint8_t) ((frame->digits[frame->digit_at] + 1) % 10);
            break;
        case MENU_KEY_PREV:
            frame->digits[frame->digit_at] = (uint8_t) ((frame->digits[frame->digit_at] + 9) % 10);
            break;
        case MENU_KEY_ENTER:
            if (frame->digit_at == MENU_EDIT_DIGITS - 1) {
                edit_confirm(frame);
                return;
            }
            frame->digit_at += 1;
            break;
        case MENU_KEY_BACK:
            if (frame->digit_at == 0) {
                pop();      // left unchanged
                return;
            }
            frame->digit_at -= 1;
            break;
    }
}


// --- A job's screen: what a click does, by the job and its state (menu_design.md, "The jobs' screens")

typedef enum {
    JOB_ACTION_ZERO,
    JOB_ACTION_TARGET,
    JOB_ACTION_POWDER,
    JOB_ACTION_ACKNOWLEDGE,
} job_action_t;


static uint8_t charge_actions(job_action_t *out) {
    charge_status_t charge;
    charge_mode_get_status(&charge);
    uint8_t count = 0;
    if (strcmp(charge.state, "fault") == 0) {
        out[count++] = JOB_ACTION_ACKNOWLEDGE;
        return count;
    }
    bool zeroing = strcmp(charge.state, "zeroing") == 0;
    bool cup_return = strcmp(charge.state, "waiting_for_cup_return") == 0;
    bool cup_removal = strcmp(charge.state, "waiting_for_cup_removal") == 0;
    // A zero while the charge waits for zero, to tare the cup on the pan. Not while it waits for the cup's return:
    // the charge takes a reading of 0 as the cup back, so a zero with the pan bare would start a throw on it. The API
    // refuses that zero for the same reason (review of ADR-010's gate 4)
    if (zeroing) {
        out[count++] = JOB_ACTION_ZERO;
    }
    // Between throws only: the API refuses both in a throw
    if ((cup_return || cup_removal) && !activity_in_throw()) {
        out[count++] = JOB_ACTION_TARGET;
        out[count++] = JOB_ACTION_POWDER;
    }
    return count;
}


static const char *job_action_name(job_action_t action) {
    switch (action) {
        case JOB_ACTION_ZERO: return "Zero";
        case JOB_ACTION_TARGET: return "Target";
        case JOB_ACTION_POWDER: return "Powder";
        case JOB_ACTION_ACKNOWLEDGE: return "OK";
    }
    return "";
}


static void push_choice(const menu_row_t *row);


static void job_key(frame_t *frame, menu_key_t key) {
    if (frame->job == ACTIVITY_UPDATE) {
        update_status_t update;
        update_status(&update);
        if (key == MENU_KEY_BACK && update.state != UPDATE_INSTALLING) {
            activity_request_stop(ACTIVITY_UPDATE, ACTIVITY_BY_BUTTON);
        }
        return;
    }
    if (key == MENU_KEY_BACK) {
        activity_request_stop(frame->job, ACTIVITY_BY_BUTTON);
        return;
    }

    if (frame->job == ACTIVITY_CLEANUP) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        if (strcmp(cleanup.state, "fault") == 0) {
            activity_request_stop(ACTIVITY_CLEANUP, ACTIVITY_BY_BUTTON);     // acknowledged
        }
        else if (key == MENU_KEY_NEXT) {
            cleanup_mode_set_speed(cleanup.speed_rps + 1.0f, NAN);          // until stopped, as a person's
        }
        else if (key == MENU_KEY_PREV) {
            cleanup_mode_set_speed(cleanup.speed_rps - 1.0f, NAN);
        }
        else {
            cleanup_mode_set_speed(0.0f, NAN);
        }
        return;
    }

    if (frame->job == ACTIVITY_CHARGE) {
        job_action_t actions[4];
        uint8_t count = charge_actions(actions);
        if (count == 0) {
            return;
        }
        if (frame->job_action >= count) {
            frame->job_action = 0;
        }
        if (key == MENU_KEY_NEXT) {
            frame->job_action = (int8_t) ((frame->job_action + 1) % count);
        }
        else if (key == MENU_KEY_PREV) {
            frame->job_action = (int8_t) ((frame->job_action + count - 1) % count);
        }
        else if (key == MENU_KEY_ENTER) {
            switch (actions[frame->job_action]) {
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
                case JOB_ACTION_TARGET:
                    push_edit(ROW_TARGET);
                    break;
                case JOB_ACTION_POWDER:
                    push_choice(ROW_POWDER);
                    break;
                case JOB_ACTION_ACKNOWLEDGE:
                    activity_request_stop(ACTIVITY_CHARGE, ACTIVITY_BY_BUTTON);
                    break;
            }
        }
    }
}


// --- A choice's list

static void push_choice(const menu_row_t *row) {
    frame_t *frame = push(FRAME_CHOICE);
    frame->row = row;
    // Focus on the current option, among those offered
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


// --- Keys on a list: a page, a choice, a message

static void list_key(frame_t *frame, menu_key_t key, int count) {
    switch (key) {
        case MENU_KEY_NEXT:
            if (frame->focus < count - 1) {
                frame->focus += 1;
            }
            return;
        case MENU_KEY_PREV:
            if (frame->focus > -1) {
                frame->focus -= 1;
            }
            return;
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


void menu_nav_key(menu_key_t key) {
    frame_t *frame = top();
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
    frame_t *frame = push(FRAME_JOB);
    frame->job = job;
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
        push_message(fault != NULL ? fault : "Motor drivers", "Charge and cleanup", "cannot start");
    }
}


void menu_nav_refresh(void) {
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
    bool job_remote = (job == ACTIVITY_CHARGE || job == ACTIVITY_CLEANUP || job == ACTIVITY_UPDATE) &&
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


static void screen_page(menu_screen_t *screen, const frame_t *frame) {
    const menu_page_t *page = frame->page;
    snprintf(screen->id, sizeof(screen->id), "%s", page->id);
    snprintf(screen->title, sizeof(screen->title), "%s", page->title_now != NULL ? page->title_now() : page->title);
    screen->title_back = frame_depth > 1;
    const menu_row_t *rows[MENU_ROWS_MAX];
    uint8_t count = shown_rows(page, rows);
    for (uint8_t idx = 0; idx < count; idx += 1) {
        const menu_row_t *row = rows[idx];
        char value[MENU_TEXT] = "";
        if (row->value != NULL) {
            row->value(row, value, sizeof(value));
        }
        bool opens = row->kind == ROW_PAGE || row->kind == ROW_CHOICE || row->kind == ROW_NUMBER;
        screen_row(screen, row->label_now != NULL ? row->label_now() : row->label, value, opens);
    }
    screen->focus = frame->focus < count ? frame->focus : (int8_t) (count - 1);
}


static void screen_choice(menu_screen_t *screen, const frame_t *frame) {
    const menu_row_t *row = frame->row;
    snprintf(screen->id, sizeof(screen->id), "choice");
    snprintf(screen->title, sizeof(screen->title), "%s", row->label);
    screen->title_back = true;
    int now = row_choice_now(row);
    for (int idx = 0; idx < row_choice_count(row); idx += 1) {
        const char *label = row_choice_label(row, idx);
        if (label != NULL) {
            screen_row(screen, label, idx == now ? "*" : "", false);
        }
    }
    screen->focus = frame->focus;
}


static void screen_edit(menu_screen_t *screen, const frame_t *frame) {
    screen->kind = MENU_SCREEN_EDIT;
    snprintf(screen->id, sizeof(screen->id), "edit");
    snprintf(screen->title, sizeof(screen->title), "%s", frame->row->label);
    screen->title_back = true;
    // The digits with their point: 024.35 or 24.350
    size_t len = 0;
    int whole = MENU_EDIT_DIGITS - frame->decimals;
    for (int idx = 0; idx < MENU_EDIT_DIGITS; idx += 1) {
        if (idx == whole) {
            screen->edit_text[len++] = '.';
        }
        if (idx == frame->digit_at) {
            screen->edit_at = (int8_t) len;
        }
        screen->edit_text[len++] = (char) ('0' + frame->digits[idx]);
    }
    screen->edit_text[len] = '\0';
}


static void screen_message(menu_screen_t *screen, const frame_t *frame) {
    snprintf(screen->id, sizeof(screen->id), "message");
    snprintf(screen->title, sizeof(screen->title), "%s", frame->message[0]);
    screen_row(screen, frame->message[1], "", false);
    if (frame->message[2][0] != '\0') {
        screen_row(screen, frame->message[2], "", false);
    }
    screen_row(screen, "OK", "", false);
    screen->focus = (int8_t) (screen->row_count - 1);
}


static const char *charge_state_text(const char *state) {
    if (strcmp(state, "zeroing") == 0) {
        return "Waiting for zero";
    }
    if (strcmp(state, "charging") == 0) {
        return "Charging";
    }
    if (strcmp(state, "waiting_for_cup_removal") == 0) {
        return "Remove the cup";
    }
    if (strcmp(state, "waiting_for_cup_return") == 0) {
        return "Return the cup";
    }
    if (strcmp(state, "starting") == 0) {
        return "Starting";
    }
    return state;
}


static void screen_job(menu_screen_t *screen, frame_t *frame) {
    screen->kind = MENU_SCREEN_JOB;
    char weight[16];
    format_weight(scale_get_current_measurement(), weight, sizeof(weight));
    const char *by = activity_job_by() == ACTIVITY_BY_API ? ", from the web" :
                     (activity_job_by() == ACTIVITY_BY_CONSOLE ? ", from the console" : "");

    if (frame->job == ACTIVITY_CHARGE) {
        charge_status_t charge;
        charge_mode_get_status(&charge);
        snprintf(screen->id, sizeof(screen->id), "job/charge");
        snprintf(screen->title, sizeof(screen->title), "Charge%s", by);
        if (strcmp(charge.state, "fault") == 0) {
            snprintf(screen->job_state, MENU_TEXT, "%s", charge_mode_get_title());
        }
        else {
            snprintf(screen->job_state, MENU_TEXT, "%s%s", charge_state_text(charge.state),
                     charge.paused ? ", paused" : "");
        }
        snprintf(screen->job_big, sizeof(screen->job_big), "%s", weight);
        char target[16];
        format_weight(charge.target, target, sizeof(target));
        snprintf(screen->job_line[0], MENU_TEXT, "Target %s  %.1f s", target, (double) charge.elapsed_s);
        snprintf(screen->job_line[1], MENU_TEXT, "%s", profile_get_name(profile_get_selected_idx()));
        if (charge.last_throw > 0 && charge.last_result != EVENT_RESULT_NONE) {
            char last[16];
            format_weight(charge.last_weight, last, sizeof(last));
            const char *verdict = charge.last_result == EVENT_RESULT_GOOD ? "good" :
                                  (charge.last_result == EVENT_RESULT_UNDER ? "under" : "over");
            snprintf(screen->job_line[2], MENU_TEXT, "#%u %s %s", (unsigned) charge.last_throw, last, verdict);
        }
        job_action_t actions[4];
        uint8_t count = charge_actions(actions);
        if (frame->job_action >= count) {
            frame->job_action = 0;
        }
        snprintf(screen->job_action, MENU_TEXT, "%s", count > 0 ? job_action_name(actions[frame->job_action]) : "");
        screen->job_actions_more = count > 1;
        screen->job_back_stops = true;
        return;
    }

    if (frame->job == ACTIVITY_CLEANUP) {
        cleanup_status_t cleanup;
        cleanup_mode_get_status(&cleanup);
        snprintf(screen->id, sizeof(screen->id), "job/cleanup");
        snprintf(screen->title, sizeof(screen->title), "Cleanup%s", by);
        bool fault = strcmp(cleanup.state, "fault") == 0;
        snprintf(screen->job_state, MENU_TEXT, "%s", fault ? cleanup_mode_get_title() : "Turn to run");
        snprintf(screen->job_big, sizeof(screen->job_big), "%.1f rev/s", (double) cleanup.speed_rps);
        snprintf(screen->job_line[0], MENU_TEXT, "Weight %s", weight);
        snprintf(screen->job_line[1], MENU_TEXT, "Gate %s", gate_state_to_string(servo_gate.gate_state));
        snprintf(screen->job_action, MENU_TEXT, "%s", fault ? "OK" : "Stop the motors");
        screen->job_back_stops = true;
        return;
    }

    update_status_t update;
    update_status(&update);
    snprintf(screen->id, sizeof(screen->id), "job/update");
    snprintf(screen->title, sizeof(screen->title), "Firmware update");
    switch (update.state) {
        case UPDATE_RECEIVING:
            if (update.expected > 0) {
                snprintf(screen->job_state, MENU_TEXT, "Receiving %lu%%",
                         (unsigned long) ((uint64_t) update.received * 100u / update.expected));
            }
            else {
                snprintf(screen->job_state, MENU_TEXT, "Waiting for the image");
            }
            break;
        case UPDATE_READY:
            snprintf(screen->job_state, MENU_TEXT, "Ready to install");
            break;
        case UPDATE_INSTALLING:
            snprintf(screen->job_state, MENU_TEXT, "Installing, restarting");
            break;
        default:
            snprintf(screen->job_state, MENU_TEXT, "Waiting for the image");
            break;
    }
    screen->job_back_stops = update.state != UPDATE_INSTALLING;
    snprintf(screen->job_line[0], MENU_TEXT, "Keep the power on.");
    snprintf(screen->job_line[1], MENU_TEXT, "Running %s", update.version);
}


void menu_nav_screen(menu_screen_t *screen) {
    memset(screen, 0, sizeof(*screen));
    screen->kind = MENU_SCREEN_LIST;
    screen->focus = -1;
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
