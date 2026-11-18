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
    bool (*save)(void);
    bool (*is_saved)(void);
} settings_resource_t;

#define SETTINGS_RESOURCES_MAX 12
#define SETTINGS_STRUCT_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

// A module's table, registered at its start, before settings_add_routes()
bool settings_register(const settings_resource_t *resource);

// The API's routes for every table registered, and the settings' own: the export, a save of all, the schema
void settings_add_routes(void);

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

#ifdef __cplusplus
}
#endif

#endif  // SETTINGS_H_
