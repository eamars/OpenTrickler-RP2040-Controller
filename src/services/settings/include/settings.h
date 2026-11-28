#ifndef SETTINGS_H_
#define SETTINGS_H_
/*
 * Each setting described once (ADR-003, gate 3; api_v1.md, "Settings"). A module describes its settings in a table:
 * for each its name, type, unit, limits or choices, access, when it takes effect, a line on what it is, and where it
 * lies in the module's struct. One code serves every table: the GET made from it, the PATCH checked whole and
 * written under the activity lock (activity.h), the module's save, `saved` from the stored CRC (common.h), the
 * export, the schema, and the console's `setting`.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

typedef enum {
    SETTING_NUMBER,     // a float, or a whole number in an integer (held_as_integer)
    SETTING_INTEGER,    // a whole number, held in an unsigned integer of `size` bytes
    SETTING_BOOLEAN,
    SETTING_STRING,     // char[size], with its ending
    SETTING_COLOUR,     // an rgbw_u32_t: "#rrggbb", its white kept as it is
    SETTING_CHOICE,     // one of `choices`, held in an integer of `size` bytes (an enum)
} setting_type_t;

typedef struct {
    int32_t held;           // as the struct holds it
    const char *text;       // the value in JSON when a string ("and_fxi"); NULL when it is a number
    int32_t number;         // the value in JSON when a number (2, 9600)
    const char *label;      // for a person
    const char *short_label;    // where the label is too long beside its field's (the screen's "8N1"); NULL: label
} setting_choice_t;

typedef enum {
    SETTING_READ_WRITE = 0,
    SETTING_READ_ONLY,
    SETTING_WRITE_ONLY,     // never in a reply nor the export (the Wi-Fi key)
} setting_access_t;

typedef struct {
    const char *name;
    const char *label;
    const char *group;      // a form gathers by it
    const char *about;      // a line on what it is
    setting_type_t type;
    uint16_t offset;        // in the struct
    uint8_t size;           // the bytes the struct gives it
    bool held_as_integer;   // a NUMBER held in an unsigned integer of `size` bytes: whole numbers only
    setting_access_t access;
    const char *unit;       // NULL: none
    float min;              // NAN: none
    float max;              // NAN: none
    float step;             // 0: none
    const setting_choice_t *choices;
    uint8_t choice_count;
    uint8_t length_min;     // a STRING's bytes, at least
    const char *applies;    // "now", "at_restart", or what the field says
    bool nullable;          // null is a value of it, held as null_held
    float null_held;
    // A CHOICE held as a value no choice has is kept as it is, shown as unknown (null; "?" on the screen): its module
    // refuses it itself (the scale's make reads nothing). Otherwise its first choice runs in its place, not stored
    bool unknown_kept;
    // For a client with little room (ADR-010, gate 6: the screen; ADR-008's pages may use them too)
    const char *short_label;    // where the label and a value do not fit 128 px; NULL: label
    float edit_step;            // what a detent (or an arrow) moves a number by; 0: its step, else none offered
    const char *on_label;       // a boolean's value in words ("Reversed"); NULL: On
    const char *off_label;      // NULL: Off
} setting_field_t;

// Where a field lies: SETTING_AT(eeprom_charge_mode_data_t, fine_stop_threshold)
#define SETTING_AT(type, member) .offset = offsetof(type, member), .size = sizeof(((type *) 0)->member)
#define SETTING_CHOICES(list) .choices = list, .choice_count = sizeof(list) / sizeof((list)[0])
#define SETTING_FIELDS(list) .fields = list, .field_count = sizeof(list) / sizeof((list)[0])

typedef enum {
    SETTINGS_REFUSED_NEVER = 0,
    SETTINGS_REFUSED_IN_THROW,      // while a throw is under way
    SETTINGS_REFUSED_A_JOB_RUNS,    // while a job runs: the charge, between throws too, or a cleanup
} settings_refused_t;

typedef struct {
    const char *path;           // "/api/v1/settings/charge"; "{id}" for one of several ("/api/v1/powder_profiles/{id}")
    const char *save_path;      // where it is stored; resources stored as one block share it
    const char *revert_path;    // the save's, /revert in place of /save: what is stored put back (ADR-008, gate 2)
    uint16_t addr;              // where its part lies in the EEPROM, as stored: its id's at addr + id * size
    const char *label;
    const char *about;
    const setting_field_t *fields;
    uint8_t field_count;
    settings_refused_t refused_while;
    size_t size;                // of the struct its fields lie in
    int32_t id_count;           // for "{id}": the ids are 0 to id_count - 1
    void *(*live)(int32_t id);  // the struct in RAM; id is -1 for a resource without one
    // What crosses fields, on the struct as it would be: NULL when it is sound, else why, and which field
    const char *(*check)(const void *proposed, int32_t id, const char **field);
    // What follows a change, once it is written in; before is the struct as it was
    void (*apply)(const void *before, int32_t id);
    // A refusal of its own, under the activity lock (the profile in use, during a throw): NULL when none, else why
    const char *(*refuse)(int32_t id);
    // What its block holds beside its parts (the charge profiles' choice), for a revert: asked first with write
    // false, NULL when it may be put back, else why it is refused; then with write true, to put it back. NULL: none
    const char *(*revert_rest)(bool write);
    bool (*save)(void);
    bool (*is_saved)(void);
} settings_resource_t;

#define SETTINGS_RESOURCES_MAX 12
#define SETTINGS_STRUCT_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

// A module's table, registered at its start, before the API adds its routes
bool settings_register(const settings_resource_t *resource);

/*
 * The settings' answers to the API (ADR-006, gate 6). The API has the routes and the web server; the settings make
 * each answer as JSON in a buffer of the API's, and name no web. A request is its path and its body as they came.
 */
typedef struct {
    const char *path;
    const char *body;               // NULL when there is none
    size_t body_len;
} settings_request_t;
typedef struct {
    char *out;                      // in: where the JSON goes, and its room
    size_t room;
    const char *job_state;          // in: the running job's state by the API's name, which a busy answer carries
    int status;                     // out: the HTTP status
    size_t len;                     // out: the JSON's length
    const char *error_code;         // out: not NULL for a plain error in place of JSON (the status, this code and
    char error_message[112];        //      this message), worded by the API as its other errors
} settings_answer_t;
// GET and PATCH of a resource's path; POST of a save's or a revert's path; POST /api/v1/settings/save
void settings_api_get(const settings_request_t *request, settings_answer_t *answer);
void settings_api_patch(const settings_request_t *request, settings_answer_t *answer);
void settings_api_save(const settings_request_t *request, settings_answer_t *answer);
void settings_api_save_all(const settings_request_t *request, settings_answer_t *answer);
void settings_api_revert(const settings_request_t *request, settings_answer_t *answer);

/*
 * The export (GET /api/v1/settings: every resource in one document) and the schema (GET /api/v1/schema: every
 * resource's fields), each made a chunk at a time: its context begun, then `more` called until it returns false
 * with the last chunk (`more` has the shape of a web server's stream).
 */
typedef struct {
    uint8_t phase;                  // 0 its head, 1 the resources, 2 its end
    uint8_t resource;
    int16_t id;
    char unit_id[16];               // the unit's id, which the export's head carries
} settings_export_t;
void settings_export_begin(settings_export_t *export_context, const char *unit_id);
bool settings_export_more(void *export_context, char *buf, size_t room, size_t *len);
// The API's own share of the schema, an item at a time: part 0 the commands' actions, 1 the states, 2 the events,
// 3 the throw record's fields; each a JSON text, NULL past a part's last
typedef const char *(*settings_schema_extra_fn)(unsigned part, size_t idx);
typedef struct {
    uint8_t phase;                  // 0 its head, 1 the resources, 2 the saves, 3 to 6 the API's parts, 7 the errors
    uint8_t resource;
    uint8_t field;                  // 0: the resource's head next
    settings_schema_extra_fn extra;
} settings_schema_t;
void settings_schema_begin(settings_schema_t *schema, settings_schema_extra_fn extra);
bool settings_schema_more(void *schema, char *buf, size_t room, size_t *len);

// Each registered table in turn, for the index (api_v1.c): NULL past the last
const settings_resource_t *settings_resource_at(size_t idx);

// A setting that applies at the next start differs from what this start read (gate 4: `restart_pending`)
bool settings_restart_pending(void);

/*
 * The console's `setting` (remote_control.c): `setting` alone lists the resources; `setting <path>` writes each
 * field as an "@setting name=value" line; `setting <path> name=value ...` writes first; `setting <path> save`
 * stores it. The path with or without "/api/v1/". The reply is "@ok setting ... saved=..." or "@err <code> ...".
 */
typedef void (*settings_line_fn)(const char *line, void *ctx);
void settings_console(const char *args, char *reply, size_t reply_size, settings_line_fn write, void *ctx);

/*
 * The screen's (ADR-010, gate 4). A path as the API's, with or without "/api/v1/" ("settings/scale"). A field's
 * description, NULL when there is none. Its value as text, as the API gives it (a choice's text or number, a
 * number, true or false), without a string's quotes. A field written as the console writes it, through the same
 * checks and refusals as the API's PATCH, and its resource stored if that changed it: NULL when done, else why not.
 */
const setting_field_t *settings_field(const char *path, const char *name);
bool settings_read_text(const char *path, const char *name, char *out, size_t room);
const char *settings_write_and_save(const char *path, const char *name, const char *value);

// The same write, live and not stored (a value set in place, a detent at a time: ADR-010, gate 6); and the store of
// a path's resource, if it changed. NULL when done, else why not
const char *settings_write(const char *path, const char *name, const char *value);
const char *settings_store(const char *path);
// The same two, by whom: a job's records kept by the API, the console or a button (the event log's `by`)
const char *settings_write_by(const char *path, const char *name, const char *value, activity_by_t by);
const char *settings_store_by(const char *path, activity_by_t by);
/*
 * Several fields written as the console writes them, by whom (the set-up's join: the network it joined, by the API
 * or the console), and the resource stored: NULL when done, else why not. Then what the resource holds now is in
 * effect, as if the unit had started with it: a network the station joined needs no restart (restart_pending)
 */
#define SETTINGS_WRITE_BY_MAX 11       // the scale's calibration: its settle time and its five lags' two fields
const char *settings_write_and_store_by(const char *path, const char *const names[], const char *const values[],
                                        size_t count, activity_by_t by);
void settings_in_effect(const char *path);
// A save's or a revert's usage for its own GET (ADR-008, gate 2), into out; false when the path is neither
bool settings_action_usage(const char *path, char *out, size_t room, size_t *len);

#ifdef __cplusplus
}
#endif

#endif  // SETTINGS_H_
