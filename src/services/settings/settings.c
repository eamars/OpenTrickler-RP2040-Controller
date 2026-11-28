/*
 * Each setting described once (ADR-003, gate 3); see settings.h.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "activity.h"
#include "common.h"
#include "eeprom.h"
#include "events.h"
#include "http_json.h"
#include "settings.h"
#include "version.h"

#define SETTINGS_API_PREFIX "/api/v1/"
#define SETTINGS_LOCK_WAIT_MS 100       // a write waits this long for one under way, or for a job's start
#define SETTINGS_FIELDS_MAX 64
#define SETTINGS_TEXT_MAX 72            // the longest string a field holds (the Wi-Fi key, 64), and its ending

static const settings_resource_t *resources[SETTINGS_RESOURCES_MAX];
static size_t resource_count = 0;


/*
    The fields that apply at the next start, as they were when this start read them (gate 4): one that differs now
    is a restart pending (GET /api/v1/system). Taken as each module registers, right after it read its settings
*/
#define SETTINGS_BOOT_BYTES 256
static uint8_t settings_boot_bytes[SETTINGS_BOOT_BYTES];
static size_t settings_boot_used = 0;


static bool applies_at_restart(const setting_field_t *field) {
    return field->applies != NULL && strcmp(field->applies, "at_restart") == 0;
}

static bool mend_choices(const settings_resource_t *resource, int32_t id);


bool settings_register(const settings_resource_t *resource) {
    if (resource_count >= SETTINGS_RESOURCES_MAX || resource->size > SETTINGS_STRUCT_MAX ||
        resource->field_count > SETTINGS_FIELDS_MAX) {
        printf("Settings: no room for %s\n", resource->path);
        return false;
    }
    // Mended in RAM only: the stored value stays until the person stores the resource (ADR-017, gate 1)
    if (resource->id_count == 0) {
        mend_choices(resource, -1);
    }
    for (int32_t id = 0; id < resource->id_count; id += 1) {
        mend_choices(resource, id);
    }
    if (resource->id_count == 0) {
        const uint8_t *base = resource->live(-1);
        for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
            const setting_field_t *field = &resource->fields[idx];
            if (applies_at_restart(field) && settings_boot_used + field->size <= SETTINGS_BOOT_BYTES) {
                memcpy(settings_boot_bytes + settings_boot_used, base + field->offset, field->size);
                settings_boot_used += field->size;
            }
        }
    }
    resources[resource_count] = resource;
    resource_count += 1;
    return true;
}


bool settings_restart_pending(void) {
    size_t used = 0;
    for (size_t idx = 0; idx < resource_count; idx += 1) {
        const settings_resource_t *resource = resources[idx];
        if (resource->id_count != 0) {
            continue;
        }
        const uint8_t *base = resource->live(-1);
        for (uint8_t field_idx = 0; field_idx < resource->field_count; field_idx += 1) {
            const setting_field_t *field = &resource->fields[field_idx];
            if (!applies_at_restart(field) || used + field->size > SETTINGS_BOOT_BYTES) {
                continue;
            }
            if (memcmp(settings_boot_bytes + used, base + field->offset, field->size) != 0) {
                return true;
            }
            used += field->size;
        }
    }
    return false;
}


const settings_resource_t *settings_resource_at(size_t idx) {
    return idx < resource_count ? resources[idx] : NULL;
}


static bool resource_has_ids(const settings_resource_t *resource) {
    return strstr(resource->path, "{id}") != NULL;
}


// The resource a path names, with its id (-1 for one without): NULL when none, or when the id is not one of its
static const settings_resource_t *resource_for_path(const char *path, int32_t *id) {
    for (size_t idx = 0; idx < resource_count; idx += 1) {
        const settings_resource_t *resource = resources[idx];
        const char *mark = strstr(resource->path, "{id}");
        if (mark == NULL) {
            if (strcmp(resource->path, path) == 0) {
                *id = -1;
                return resource;
            }
            continue;
        }
        size_t prefix_len = (size_t) (mark - resource->path);
        const char *suffix = mark + 4;
        if (strncmp(resource->path, path, prefix_len) != 0) {
            continue;
        }
        const char *digits = path + prefix_len;
        char *end = NULL;
        long value = strtol(digits, &end, 10);
        if (end == digits || *digits < '0' || *digits > '9' || strcmp(end, suffix) != 0) {
            continue;
        }
        if (value < 0 || value >= resource->id_count) {
            return NULL;
        }
        *id = (int32_t) value;
        return resource;
    }
    return NULL;
}


// --- Values as the structs hold them

static uint32_t held_uint(const uint8_t *at, uint8_t size) {
    switch (size) {
        case 1:
            return at[0];
        case 2: {
            uint16_t value;
            memcpy(&value, at, sizeof(value));
            return value;
        }
        default: {
            uint32_t value;
            memcpy(&value, at, sizeof(value));
            return value;
        }
    }
}


/*
 * A stored choice that is none of its choices is no setting: its first choice runs in its place, in RAM only, and
 * the console says so; the resource then reads as not stored, and the chip keeps what it held until the person stores
 * it (ADR-017, gate 1: a load never writes). A field whose module refuses such a value itself is left as it is
 * (unknown_kept). True when one was mended. A field added into a struct's filler reads whatever byte was stored there
 * (ADR-006, handed on by ADR-010): the display's Charge view read neither 0 nor 1 on the Pico W
 */
static void put_uint(uint8_t *at, uint8_t size, uint32_t value);

static bool mend_choices(const settings_resource_t *resource, int32_t id) {
    uint8_t *base = resource->live(id);
    bool mended = false;
    for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
        const setting_field_t *field = &resource->fields[idx];
        if (field->type != SETTING_CHOICE || field->choice_count == 0 || field->unknown_kept) {
            continue;
        }
        uint32_t held = held_uint(base + field->offset, field->size);
        bool known = false;
        for (uint8_t choice = 0; choice < field->choice_count && !known; choice += 1) {
            known = held == (uint32_t) field->choices[choice].held;
        }
        if (!known) {
            printf("Settings: %s %s held %lu, none of its choices: %s runs in its place, not stored\n",
                   resource->path, field->name, (unsigned long) held, field->choices[0].label);
            put_uint(base + field->offset, field->size, (uint32_t) field->choices[0].held);
            mended = true;
        }
    }
    return mended;
}


static void put_uint(uint8_t *at, uint8_t size, uint32_t value) {
    switch (size) {
        case 1:
            at[0] = (uint8_t) value;
            break;
        case 2: {
            uint16_t narrow = (uint16_t) value;
            memcpy(at, &narrow, sizeof(narrow));
            break;
        }
        default:
            memcpy(at, &value, sizeof(value));
            break;
    }
}


static float held_float(const uint8_t *at) {
    float value;
    memcpy(&value, at, sizeof(value));
    return value;
}


static const setting_choice_t *choice_held(const setting_field_t *field, uint32_t held) {
    for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
        if ((uint32_t) field->choices[idx].held == held) {
            return &field->choices[idx];
        }
    }
    return NULL;
}


// A choice's value in JSON
static void write_choice_value(const setting_choice_t *choice, char *out, size_t room, size_t *len) {
    if (choice->text != NULL) {
        http_json_string(out, room, len, choice->text);
    }
    else {
        http_json_integer(out, room, len, choice->number);
    }
}


// A field's value, as the struct at base holds it, in JSON
static void write_value(const setting_field_t *field, const uint8_t *base, char *out, size_t room, size_t *len) {
    const uint8_t *at = base + field->offset;
    switch (field->type) {
        case SETTING_NUMBER:
            if (field->held_as_integer) {
                http_json_integer(out, room, len, (long) held_uint(at, field->size));
            }
            else if (field->nullable && held_float(at) == field->null_held) {
                http_json_raw(out, room, len, "null");
            }
            else {
                http_json_float(out, room, len, held_float(at));
            }
            break;
        case SETTING_INTEGER:
            http_json_integer(out, room, len, (long) held_uint(at, field->size));
            break;
        case SETTING_BOOLEAN:
            http_json_bool(out, room, len, at[0] != 0);
            break;
        case SETTING_STRING: {
            // A string stored with no ending (a profile's name of 16, before gate 3) is read to its room only
            char text[SETTINGS_TEXT_MAX];
            size_t text_len = strnlen((const char *) at, field->size);
            if (text_len >= sizeof(text)) {
                text_len = sizeof(text) - 1;
            }
            memcpy(text, at, text_len);
            text[text_len] = '\0';
            http_json_string(out, room, len, text);
            break;
        }
        case SETTING_COLOUR: {
            char text[12];
            snprintf(text, sizeof(text), "\"#%06lx\"", (unsigned long) (held_uint(at, 4) & 0xffffffu));
            http_json_raw(out, room, len, text);
            break;
        }
        case SETTING_CHOICE: {
            const setting_choice_t *choice = choice_held(field, held_uint(at, field->size));
            if (choice == NULL) {
                http_json_raw(out, room, len, "null");     // stored before the choices were checked
            }
            else {
                write_choice_value(choice, out, room, len);
            }
            break;
        }
    }
}


// Where a resource's part lies in the EEPROM: one of several ({id}) after the others
static uint16_t part_addr(const settings_resource_t *resource, int32_t id) {
    return (uint16_t) (resource->addr + (id > 0 ? (size_t) id * resource->size : 0));
}


/*
 * Saved, for the resource's own part of its record (ADR-008, gate 2): a powder of the eight, a motor of the two.
 * The record as stored says so at once; otherwise this part against what the EEPROM holds of it, read only then,
 * so a value written back as stored is stored again. The EEPROM is the truth: no copy of it is kept to drift
 */
static bool part_saved(const settings_resource_t *resource, int32_t id) {
    if (resource->is_saved()) {
        return true;
    }
    uint8_t stored[SETTINGS_STRUCT_MAX];
    if (resource->size > sizeof(stored) || !config_read_part(part_addr(resource, id), stored, resource->size)) {
        return false;
    }
    return memcmp(stored, resource->live(id), resource->size) == 0;
}


// A resource as its GET gives it: saved, then each field a reply may carry
static void write_resource(const settings_resource_t *resource, int32_t id, char *out, size_t room, size_t *len) {
    const uint8_t *base = resource->live(id);
    http_json_raw(out, room, len, "{\"saved\":");
    http_json_bool(out, room, len, part_saved(resource, id));
    for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
        const setting_field_t *field = &resource->fields[idx];
        if (field->access == SETTING_WRITE_ONLY) {
            continue;
        }
        http_json_raw(out, room, len, ",");
        http_json_string(out, room, len, field->name);
        http_json_raw(out, room, len, ":");
        write_value(field, base, out, room, len);
    }
    http_json_raw(out, room, len, "}");
}


// --- A write

typedef struct {
    int status;
    const char *code;
    const setting_field_t *field;
    char field_name[40];        // the request's, for a field that is none of the resource's
    char message[112];
    char ignored[64];           // the names a write ignored, each ended by '\0' (forward compatibility)
    uint8_t ignored_count;      // how many fit in it
    bool ignored_more;          // and some did not
} outcome_t;


// A name the resource does not have, kept for the reply: a later version's field, or a typo the reply shows
static void outcome_ignore(outcome_t *outcome, const char *name, size_t name_len) {
    size_t used = 0;
    for (uint8_t idx = 0; idx < outcome->ignored_count; idx += 1) {
        used += strlen(outcome->ignored + used) + 1;
    }
    if (used + name_len + 1 > sizeof(outcome->ignored)) {
        outcome->ignored_more = true;
        return;
    }
    memcpy(outcome->ignored + used, name, name_len);
    outcome->ignored[used + name_len] = '\0';
    outcome->ignored_count += 1;
}


static void fail(outcome_t *outcome, int status, const char *code, const setting_field_t *field, const char *format,
                 ...) {
    outcome->status = status;
    outcome->code = code;
    outcome->field = field;
    va_list args;
    va_start(args, format);
    vsnprintf(outcome->message, sizeof(outcome->message), format, args);
    va_end(args);
}


/*
 * A member's value as text: a JSON string's, decoded; the console's, as typed. The console types every value as
 * text, and the field's type reads it: kind is then what the text reads as (a number, true or false, null, text).
 */
static bool member_text(const http_json_member_t *member, bool console, char *out, size_t room) {
    if (!console) {
        return http_json_string_value(member, out, room);
    }
    if (member->value_len >= room) {
        return false;
    }
    memcpy(out, member->value, member->value_len);
    out[member->value_len] = '\0';
    return true;
}


static http_json_kind_t member_kind(const http_json_member_t *member, bool console) {
    if (!console) {
        return member->kind;
    }
    char text[SETTINGS_TEXT_MAX];
    if (!member_text(member, true, text, sizeof(text))) {
        return HTTP_JSON_STRING;
    }
    if (strcmp(text, "null") == 0) {
        return HTTP_JSON_NULL;
    }
    if (strcmp(text, "true") == 0 || strcmp(text, "false") == 0) {
        return HTTP_JSON_BOOLEAN;
    }
    return http_json_is_number(text, strlen(text)) ? HTTP_JSON_NUMBER : HTTP_JSON_STRING;
}


static bool member_number(const http_json_member_t *member, bool console, double *value) {
    if (!console) {
        return http_json_number_value(member, value);
    }
    char text[SETTINGS_TEXT_MAX];
    if (!member_text(member, true, text, sizeof(text))) {
        return false;
    }
    *value = strtod(text, NULL);
    return isfinite(*value);
}


static bool parse_colour(const char *text, uint32_t *rgb) {
    if (strlen(text) != 7 || text[0] != '#') {
        return false;
    }
    for (int idx = 1; idx < 7; idx += 1) {
        char c = text[idx];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    *rgb = (uint32_t) strtoul(text + 1, NULL, 16);
    return true;
}


// A number as a message gives it: as a reply would (the SDK's printf leaves %g's zeros: "12.0000")
static const char *number_text(double value, char *buf, size_t room) {
    size_t len = 0;
    buf[0] = '\0';
    http_json_float(buf, room, &len, (float) value);
    return buf;
}


// A number against its field's limits and step; false with the outcome set
static bool number_in_limits(const setting_field_t *field, double value, outcome_t *outcome) {
    char given[24];
    char limit[24];
    bool whole = field->type == SETTING_INTEGER || field->held_as_integer;
    if (whole && value != floor(value)) {
        fail(outcome, 400, "out_of_range", field, "%s is not a whole number",
             number_text(value, given, sizeof(given)));
        return false;
    }
    if (!isnan(field->min) && value < field->min) {
        fail(outcome, 400, "out_of_range", field, "%s is below the least, %s",
             number_text(value, given, sizeof(given)), number_text(field->min, limit, sizeof(limit)));
        return false;
    }
    if (!isnan(field->max) && value > field->max) {
        fail(outcome, 400, "out_of_range", field, "%s is above the most, %s",
             number_text(value, given, sizeof(given)), number_text(field->max, limit, sizeof(limit)));
        return false;
    }
    if (field->step > 0.0f) {
        double from = isnan(field->min) ? 0.0 : field->min;
        double steps = (value - from) / field->step;
        if (fabs(steps - round(steps)) > 1e-4) {
            fail(outcome, 400, "out_of_range", field, "%s is not in steps of %s",
                 number_text(value, given, sizeof(given)), number_text(field->step, limit, sizeof(limit)));
            return false;
        }
    }
    return true;
}


// A member's value into the struct at base, checked against its field; false with the outcome set
static bool read_value(const setting_field_t *field, const http_json_member_t *member, bool console, uint8_t *base,
                       outcome_t *outcome) {
    uint8_t *at = base + field->offset;
    http_json_kind_t kind = member_kind(member, console);
    char text[SETTINGS_TEXT_MAX];

    if (kind == HTTP_JSON_NULL) {
        // A choice kept unknown reads as null: written back as read, it is left as it is (ADR-017's review, finding 6:
        // a unit's own copy restores onto it, the rest of its resource with it)
        if (field->type == SETTING_CHOICE && field->unknown_kept && choice_held(field, held_uint(at, field->size)) == NULL) {
            return true;
        }
        if (!field->nullable) {
            fail(outcome, 400, "wrong_type", field, "%s cannot be null", field->name);
            return false;
        }
        memcpy(at, &field->null_held, sizeof(float));
        return true;
    }

    switch (field->type) {
        case SETTING_NUMBER:
        case SETTING_INTEGER: {
            double value;
            if (kind != HTTP_JSON_NUMBER || !member_number(member, console, &value)) {
                fail(outcome, 400, "wrong_type", field, "%s is a number", field->name);
                return false;
            }
            if (!number_in_limits(field, value, outcome)) {
                return false;
            }
            if (field->type == SETTING_INTEGER || field->held_as_integer) {
                put_uint(at, field->size, (uint32_t) value);
            }
            else {
                float narrow = (float) value;
                memcpy(at, &narrow, sizeof(narrow));
            }
            return true;
        }
        case SETTING_BOOLEAN:
            if (kind != HTTP_JSON_BOOLEAN) {
                fail(outcome, 400, "wrong_type", field, "%s is true or false", field->name);
                return false;
            }
            at[0] = member->value[0] == 't' ? 1 : 0;
            return true;
        case SETTING_STRING: {
            if (!console && kind != HTTP_JSON_STRING) {
                fail(outcome, 400, "wrong_type", field, "%s is a string", field->name);
                return false;
            }
            if (!member_text(member, console, text, sizeof(text)) || strlen(text) + 1 > field->size) {
                fail(outcome, 400, "out_of_range", field, "%s is longer than %u bytes", field->name,
                     (unsigned) field->size - 1);
                return false;
            }
            if (strlen(text) < field->length_min) {
                fail(outcome, 400, "out_of_range", field, "%s is shorter than %u bytes", field->name,
                     (unsigned) field->length_min);
                return false;
            }
            memset(at, 0, field->size);
            memcpy(at, text, strlen(text));
            return true;
        }
        case SETTING_COLOUR: {
            uint32_t rgb;
            if ((!console && kind != HTTP_JSON_STRING) || !member_text(member, console, text, sizeof(text)) ||
                !parse_colour(text, &rgb)) {
                fail(outcome, 400, "wrong_type", field, "%s is a colour, \"#rrggbb\"", field->name);
                return false;
            }
            // The white the LEDs may have is not the API's: kept as it is
            put_uint(at, 4, (held_uint(at, 4) & 0xff000000u) | rgb);
            return true;
        }
        case SETTING_CHOICE: {
            const setting_choice_t *found = NULL;
            if (kind == HTTP_JSON_STRING && member_text(member, console, text, sizeof(text))) {
                for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
                    if (field->choices[idx].text != NULL && strcmp(field->choices[idx].text, text) == 0) {
                        found = &field->choices[idx];
                    }
                }
            }
            else if (kind == HTTP_JSON_NUMBER) {
                double value;
                if (member_number(member, console, &value)) {
                    for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
                        if (field->choices[idx].text == NULL && field->choices[idx].number == value) {
                            found = &field->choices[idx];
                        }
                    }
                }
            }
            if (found == NULL) {
                fail(outcome, 400, "not_a_choice", field, "%s is not one of its choices", field->name);
                return false;
            }
            put_uint(at, field->size, (uint32_t) found->held);
            return true;
        }
    }
    return false;
}


static int field_index(const settings_resource_t *resource, const char *name, size_t name_len) {
    for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
        const char *field_name = resource->fields[idx].name;
        if (strlen(field_name) == name_len && memcmp(field_name, name, name_len) == 0) {
            return idx;
        }
    }
    return -1;
}


// A setting's event (ADR-008, gate 2): one for each resource written or stored, so a page or the screen showing it
// reads it again. The path is the resource's (its {id} by number) or, for a store, the save's
static void setting_event(event_kind_t kind, const char *path, int32_t id, activity_by_t by) {
    event_t event = {.source = EVENT_SOURCE_SETTING, .kind = (uint8_t) kind, .by = (uint8_t) by, .text = path,
                     .number = (uint8_t) (id >= 0 ? id : 0)};
    events_add(&event);
}


// A store, its event only where it was not stored: a save that changes nothing is no news
static bool store(const settings_resource_t *resource, activity_by_t by) {
    bool was_saved = resource->is_saved();
    if (!resource->save()) {
        return false;
    }
    if (!was_saved) {
        setting_event(EVENT_STORED, resource->save_path, -1, by);
    }
    return true;
}


/*
 * A write: the whole request checked on a copy of the struct, then only its fields written into the struct in RAM,
 * all under the activity lock, so no throw or job starts half way through it (activity.h). Refused whole, it
 * changes nothing. A name the resource does not have is ignored and said (ignore_unknown: the API's and the
 * console's, so a later version's copy restores onto this one; the owner, 2026-11-26), or refused (the firmware's
 * own writes: a name it does not have is its own mistake).
 */
static void settings_patch(const settings_resource_t *resource, int32_t id, const http_json_member_t *members,
                           size_t count, bool console, bool ignore_unknown, activity_by_t by, outcome_t *outcome) {
    memset(outcome, 0, sizeof(*outcome));
    outcome->status = 200;
    if (!activity_try_lock(SETTINGS_LOCK_WAIT_MS)) {
        fail(outcome, 409, "busy", NULL, "the settings are being written");
        return;
    }

    uint8_t proposed[SETTINGS_STRUCT_MAX];
    uint8_t before[SETTINGS_STRUCT_MAX];
    uint8_t *live = resource->live(id);
    memcpy(proposed, live, resource->size);
    memcpy(before, live, resource->size);
    bool given[SETTINGS_FIELDS_MAX] = {false};
    size_t given_count = 0;

    for (size_t idx = 0; idx < count; idx += 1) {
        const http_json_member_t *member = &members[idx];
        int field_idx = field_index(resource, member->name, member->name_len);
        if (field_idx < 0 && member->name_len == 5 && memcmp(member->name, "saved", 5) == 0) {
            // In every GET, so a client may send it back with the rest: worked out, never written
            fail(outcome, 400, "read_only", NULL, "saved is worked out, not written");
            strcpy(outcome->field_name, "saved");
            goto done;
        }
        if (field_idx < 0 && ignore_unknown) {
            outcome_ignore(outcome, member->name, member->name_len);
            continue;
        }
        if (field_idx < 0) {
            size_t name_len = member->name_len < sizeof(outcome->field_name) ? member->name_len :
                              sizeof(outcome->field_name) - 1;
            memcpy(outcome->field_name, member->name, name_len);
            outcome->field_name[name_len] = '\0';
            fail(outcome, 400, "unknown_field", NULL, "%s has no field %s", resource->path, outcome->field_name);
            goto done;
        }
        const setting_field_t *field = &resource->fields[field_idx];
        if (field->access == SETTING_READ_ONLY) {
            fail(outcome, 400, "read_only", field, "%s is read only", field->name);
            goto done;
        }
        if (!read_value(field, member, console, proposed, outcome)) {
            goto done;
        }
        given[field_idx] = true;
        given_count += 1;
    }

    if (resource->check != NULL) {
        const char *field_name = NULL;
        const char *why = resource->check(proposed, id, &field_name);
        if (why != NULL) {
            const setting_field_t *field = NULL;
            if (field_name != NULL) {
                int field_idx = field_index(resource, field_name, strlen(field_name));
                field = field_idx >= 0 ? &resource->fields[field_idx] : NULL;
            }
            fail(outcome, 400, "out_of_range", field, "%s", why);
            goto done;
        }
    }

    if (resource->refused_while == SETTINGS_REFUSED_IN_THROW && activity_in_throw()) {
        fail(outcome, 409, "busy", NULL, "%s is refused while a throw is under way", resource->path);
        goto done;
    }
    if (resource->refused_while == SETTINGS_REFUSED_A_JOB_RUNS && activity_job_runs()) {
        fail(outcome, 409, "busy", NULL, "%s is refused while a job runs", resource->path);
        goto done;
    }
    if (resource->refuse != NULL) {
        const char *why = resource->refuse(id);
        if (why != NULL) {
            fail(outcome, 409, "busy", NULL, "%s", why);
            goto done;
        }
    }

    for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
        if (given[idx]) {
            const setting_field_t *field = &resource->fields[idx];
            memcpy(live + field->offset, proposed + field->offset, field->size);
        }
    }
    if (resource->apply != NULL && given_count > 0) {
        resource->apply(before, id);
    }
    if (memcmp(before, live, resource->size) != 0) {
        setting_event(EVENT_CHANGED, resource->path, id, by);
    }

done:
    activity_unlock();
}


// --- The API's replies: each made as JSON in the caller's buffer (settings.h), so the settings name no web server

static char *answer_buffer(settings_answer_t *answer, size_t *room) {
    *room = answer->room;
    return answer->out;
}


// A plain error in place of JSON: the caller words it as its other errors
static void answer_error(settings_answer_t *answer, int status, const char *code, const char *message) {
    answer->status = status;
    answer->error_code = code;
    snprintf(answer->error_message, sizeof(answer->error_message), "%s", message);
}


static void answer_send(settings_answer_t *answer, int status, size_t len) {
    answer->status = status;
    answer->len = len;
    answer->error_code = NULL;
}


// A field with limits: a number, or an integer
static bool field_is_number(const setting_field_t *field) {
    return field->type == SETTING_NUMBER || field->type == SETTING_INTEGER;
}


static void reply_outcome(settings_answer_t *reply, const outcome_t *outcome) {
    size_t room;
    char *out = answer_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"error\":{\"code\":");
    http_json_string(out, room, &len, outcome->code);
    const char *field_name = outcome->field != NULL ? outcome->field->name :
                             outcome->field_name[0] ? outcome->field_name : NULL;
    if (field_name != NULL) {
        http_json_raw(out, room, &len, ",\"field\":");
        http_json_string(out, room, &len, field_name);
    }
    http_json_raw(out, room, &len, ",\"message\":");
    http_json_string(out, room, &len, outcome->message);
    const setting_field_t *field = outcome->field;
    if (field != NULL && field_is_number(field) && strcmp(outcome->code, "out_of_range") == 0) {
        if (!isnan(field->min)) {
            http_json_raw(out, room, &len, ",\"min\":");
            http_json_float(out, room, &len, field->min);
        }
        if (!isnan(field->max)) {
            http_json_raw(out, room, &len, ",\"max\":");
            http_json_float(out, room, &len, field->max);
        }
    }
    if (field != NULL && strcmp(outcome->code, "not_a_choice") == 0) {
        http_json_raw(out, room, &len, ",\"choices\":[");
        for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
            if (idx) {
                http_json_raw(out, room, &len, ",");
            }
            write_choice_value(&field->choices[idx], out, room, &len);
        }
        http_json_raw(out, room, &len, "]");
    }
    if (strcmp(outcome->code, "busy") == 0) {
        http_json_raw(out, room, &len, ",\"activity\":");
        http_json_string(out, room, &len, activity_name(activity_get()));
        http_json_raw(out, room, &len, ",\"state\":");
        http_json_string(out, room, &len, reply->job_state);
    }
    http_json_raw(out, room, &len, ",\"see\":\"/api/v1/schema\"}}");
    if (!http_json_fits(room, len)) {
        answer_error(reply, outcome->status, outcome->code, "the error did not fit");
        return;
    }
    answer_send(reply, outcome->status, len);
}


// A write's names ignored, as "ignored": [...] at the end of the resource's object; "..." last when some did not fit
static void write_ignored(const outcome_t *outcome, char *out, size_t room, size_t *len) {
    if (*len == 0 || !http_json_fits(room, *len) || out[*len - 1] != '}') {
        return;
    }
    *len -= 1;
    http_json_raw(out, room, len, ",\"ignored\":[");
    const char *name = outcome->ignored;
    for (uint8_t idx = 0; idx < outcome->ignored_count; idx += 1) {
        http_json_raw(out, room, len, idx > 0 ? "," : "");
        http_json_string(out, room, len, name);
        name += strlen(name) + 1;
    }
    if (outcome->ignored_more) {
        http_json_raw(out, room, len, outcome->ignored_count > 0 ? ",\"...\"" : "\"...\"");
    }
    http_json_raw(out, room, len, "]}");
}


// The resource as a GET gives it; after a write, with the names it ignored (outcome NULL: none)
static void reply_resource(settings_answer_t *reply, const settings_resource_t *resource, int32_t id,
                           const outcome_t *outcome) {
    size_t room;
    char *out = answer_buffer(reply, &room);
    size_t len = 0;
    write_resource(resource, id, out, room, &len);
    if (outcome != NULL && (outcome->ignored_count > 0 || outcome->ignored_more)) {
        write_ignored(outcome, out, room, &len);
    }
    if (!http_json_fits(room, len)) {
        answer_error(reply, 500, "failed", "the reply did not fit");
        return;
    }
    answer_send(reply, 200, len);
}


void settings_api_get(const settings_request_t *request, settings_answer_t *reply) {
    int32_t id;
    const settings_resource_t *resource = resource_for_path(request->path, &id);
    if (resource == NULL) {
        answer_error(reply, 404, "not_found", "no such id");
        return;
    }
    reply_resource(reply, resource, id, NULL);
}


void settings_api_patch(const settings_request_t *request, settings_answer_t *reply) {
    int32_t id;
    const settings_resource_t *resource = resource_for_path(request->path, &id);
    if (resource == NULL) {
        answer_error(reply, 404, "not_found", "no such id");
        return;
    }
    http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
    size_t count;
    const char *why;
    if (!http_json_parse_object(request->body, request->body_len, members, HTTP_JSON_MEMBERS_MAX, &count, &why)) {
        answer_error(reply, 400, "bad_json", why);
        return;
    }
    outcome_t outcome;
    settings_patch(resource, id, members, count, false, true, ACTIVITY_BY_API, &outcome);
    if (outcome.status != 200) {
        reply_outcome(reply, &outcome);
        return;
    }
    reply_resource(reply, resource, id, &outcome);
}


// An action's body: {}, and nothing in it (api_v1.md: unknown fields are refused)
static bool empty_body(const settings_request_t *request, settings_answer_t *reply) {
    http_json_member_t member;
    size_t count;
    const char *why;
    if (!http_json_parse_object(request->body, request->body_len, &member, 1, &count, &why)) {
        answer_error(reply, 400, "bad_json", why);
        return false;
    }
    if (count > 0) {
        answer_error(reply, 400, "unknown_field", "this action takes {}");
        return false;
    }
    return true;
}


static void reply_saved(settings_answer_t *reply) {
    size_t room;
    char *out = answer_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"saved\":true}");
    answer_send(reply, 200, len);
}


void settings_api_save(const settings_request_t *request, settings_answer_t *reply) {
    if (!empty_body(request, reply)) {
        return;
    }
    for (size_t idx = 0; idx < resource_count; idx += 1) {
        if (strcmp(resources[idx]->save_path, request->path) == 0) {
            if (!store(resources[idx], ACTIVITY_BY_API)) {
                answer_error(reply, 500, "failed", "the EEPROM did not take it");
                return;
            }
            reply_saved(reply);
            return;
        }
    }
    answer_error(reply, 404, "not_found", "nothing is stored there");
}


void settings_api_save_all(const settings_request_t *request, settings_answer_t *reply) {
    if (!empty_body(request, reply)) {
        return;
    }
    for (size_t idx = 0; idx < resource_count; idx += 1) {
        // Resources stored as one block share their save: once each
        bool done_before = false;
        for (size_t earlier = 0; earlier < idx; earlier += 1) {
            done_before = done_before || resources[earlier]->save == resources[idx]->save;
        }
        if (!done_before && !store(resources[idx], ACTIVITY_BY_API)) {
            char message[96];
            snprintf(message, sizeof(message), "%s: the EEPROM did not take it", resources[idx]->save_path);
            answer_error(reply, 500, "failed", message);
            return;
        }
    }
    reply_saved(reply);
}


/*
 * A revert (ADR-008, gate 2): every resource stored at that save given back what the EEPROM holds of it, each part
 * as a write gives it (its refusals, its apply, its event where it changed). Refused whole, as a write is: the
 * refusals are looked at first, and what is stored is checked as a write's value, before anything is written
 */
static void revert(const char *revert_path, activity_by_t by, outcome_t *outcome) {
    memset(outcome, 0, sizeof(*outcome));
    outcome->status = 200;
    if (!activity_try_lock(SETTINGS_LOCK_WAIT_MS)) {
        fail(outcome, 409, "busy", NULL, "the settings are being written");
        return;
    }
    uint8_t stored[SETTINGS_STRUCT_MAX];
    for (int pass = 0; pass < 2; pass += 1) {
        for (size_t idx = 0; idx < resource_count; idx += 1) {
            const settings_resource_t *resource = resources[idx];
            if (strcmp(resource->revert_path, revert_path) != 0) {
                continue;
            }
            if (pass == 0 && resource->refused_while == SETTINGS_REFUSED_IN_THROW && activity_in_throw()) {
                fail(outcome, 409, "busy", NULL, "%s is refused while a throw is under way", resource->path);
                goto done;
            }
            if (pass == 0 && resource->refused_while == SETTINGS_REFUSED_A_JOB_RUNS && activity_job_runs()) {
                fail(outcome, 409, "busy", NULL, "%s is refused while a job runs", resource->path);
                goto done;
            }
            int32_t first = resource_has_ids(resource) ? 0 : -1;
            int32_t last = resource_has_ids(resource) ? resource->id_count - 1 : -1;
            for (int32_t id = first; id <= last; id += 1) {
                if (!config_read_part(part_addr(resource, id), stored, resource->size)) {
                    fail(outcome, 500, "failed", NULL, "the EEPROM could not be read");
                    goto done;
                }
                if (pass == 0) {
                    const char *why = resource->refuse != NULL ? resource->refuse(id) : NULL;
                    const char *field_name = NULL;
                    if (why != NULL) {
                        fail(outcome, 409, "busy", NULL, "%s", why);
                        goto done;
                    }
                    why = resource->check != NULL ? resource->check(stored, id, &field_name) : NULL;
                    if (why != NULL) {
                        fail(outcome, 409, "stored_refused", NULL, "what is stored is refused: %s", why);
                        goto done;
                    }
                    continue;
                }
                uint8_t before[SETTINGS_STRUCT_MAX];
                uint8_t *live = resource->live(id);
                memcpy(before, live, resource->size);
                memcpy(live, stored, resource->size);
                if (memcmp(before, live, resource->size) != 0) {
                    if (resource->apply != NULL) {
                        resource->apply(before, id);
                    }
                    setting_event(EVENT_CHANGED, resource->path, id, by);
                }
            }
            if (resource->revert_rest != NULL) {
                const char *why = resource->revert_rest(pass == 1);
                if (why != NULL) {
                    fail(outcome, 409, "busy", NULL, "%s", why);
                    goto done;
                }
            }
        }
    }
done:
    activity_unlock();
}


void settings_api_revert(const settings_request_t *request, settings_answer_t *reply) {
    if (!empty_body(request, reply)) {
        return;
    }
    outcome_t outcome;
    revert(request->path, ACTIVITY_BY_API, &outcome);
    if (outcome.status != 200) {
        reply_outcome(reply, &outcome);
        return;
    }
    size_t room;
    char *out = answer_buffer(reply, &room);
    size_t len = 0;
    http_json_raw(out, room, &len, "{\"reverted\":true}");
    answer_send(reply, 200, len);
}


bool settings_action_usage(const char *path, char *out, size_t room, size_t *len) {
    bool all = strcmp(path, "/api/v1/settings/save") == 0;
    for (size_t idx = 0; idx < resource_count && !all; idx += 1) {
        const settings_resource_t *resource = resources[idx];
        bool save = strcmp(path, resource->save_path) == 0;
        if (!save && strcmp(path, resource->revert_path) != 0) {
            continue;
        }
        http_json_raw(out, room, len, "{\"usage\":{\"method\":\"POST\",\"path\":");
        http_json_string(out, room, len, path);
        http_json_raw(out, room, len, save ? ",\"fields\":[],\"refused_while\":\"never\",\"about\":\"stores what "
                                             "was written to the settings stored here; answers {\\\"saved\\\": true}\"" :
                                             ",\"fields\":[],\"about\":\"puts back what is stored here, undoing what was "
                                             "written since; each resource that changed gives its setting changed event; "
                                             "409 busy as a write to it is; answers {\\\"reverted\\\": true}\"");
        http_json_raw(out, room, len, ",\"example\":{}},\"see\":\"/api/v1/schema\"}");
        return true;
    }
    if (all) {
        http_json_raw(out, room, len, "{\"usage\":{\"method\":\"POST\",\"path\":\"/api/v1/settings/save\","
                                      "\"fields\":[],\"refused_while\":\"never\",\"about\":\"stores every setting, "
                                      "the powders and the loads\",\"example\":{}},\"see\":\"/api/v1/schema\"}");
        return true;
    }
    return false;
}


// --- The export, GET /api/v1/settings: every resource in one document, made a resource at a time

// Write an item at the end of buf; false, and buf as it was, when it does not fit
static bool stream_item_fits(char *buf, size_t room, size_t *len, size_t mark) {
    if (http_json_fits(room, *len)) {
        return true;
    }
    *len = mark;
    buf[mark] = '\0';
    return false;
}


static void write_firmware(char *out, size_t room, size_t *len) {
    http_json_raw(out, room, len, "\"firmware\":{\"version\":");
    http_json_string(out, room, len, version_string);
    http_json_raw(out, room, len, ",\"vcs\":");
    http_json_string(out, room, len, vcs_hash);
    http_json_raw(out, room, len, ",\"build\":");
    http_json_string(out, room, len, build_type);
    http_json_raw(out, room, len, "}");
}


// A resource's path with its id in place of "{id}"
static void resource_path(const settings_resource_t *resource, int32_t id, char *out, size_t room) {
    const char *mark = strstr(resource->path, "{id}");
    if (mark == NULL) {
        snprintf(out, room, "%s", resource->path);
        return;
    }
    snprintf(out, room, "%.*s%ld%s", (int) (mark - resource->path), resource->path, (long) id, mark + 4);
}


void settings_export_begin(settings_export_t *export, const char *unit_id) {
    memset(export, 0, sizeof(*export));
    snprintf(export->unit_id, sizeof(export->unit_id), "%s", unit_id);
}


bool settings_export_more(void *ctx, char *buf, size_t room, size_t *len) {
    settings_export_t *export = ctx;
    *len = 0;
    if (export->phase == 0) {
        http_json_raw(buf, room, len, "{\"api\":\"1\",");
        write_firmware(buf, room, len);
        http_json_raw(buf, room, len, ",\"unit_id\":");
        http_json_string(buf, room, len, export->unit_id);
        http_json_raw(buf, room, len, ",\"resources\":{");
        export->phase = 1;
        export->resource = 0;
        export->id = 0;
        return true;
    }
    if (export->phase == 1) {
        while (export->resource < resource_count) {
            const settings_resource_t *resource = resources[export->resource];
            int32_t id = resource_has_ids(resource) ? export->id : -1;
            size_t mark = *len;
            char path[64];
            resource_path(resource, id, path, sizeof(path));
            if (export->resource > 0 || export->id > 0) {
                http_json_raw(buf, room, len, ",");
            }
            http_json_string(buf, room, len, path);
            http_json_raw(buf, room, len, ":");
            write_resource(resource, id, buf, room, len);
            if (!stream_item_fits(buf, room, len, mark)) {
                return true;    // the next chunk begins with it
            }
            if (resource_has_ids(resource) && export->id + 1 < resource->id_count) {
                export->id += 1;
            }
            else {
                export->resource += 1;
                export->id = 0;
            }
        }
        export->phase = 2;
        return true;
    }
    http_json_raw(buf, room, len, "}}");
    return false;
}


// --- The schema, GET /api/v1/schema: every resource's fields, made a field at a time

static const char *type_name(const setting_field_t *field) {
    switch (field->type) {
        case SETTING_NUMBER:
            return "number";
        case SETTING_INTEGER:
            return "integer";
        case SETTING_BOOLEAN:
            return "boolean";
        case SETTING_STRING:
            return "string";
        case SETTING_COLOUR:
            return "colour";
        case SETTING_CHOICE:
        default:
            return "choice";
    }
}


static const char *access_name(setting_access_t access) {
    switch (access) {
        case SETTING_READ_ONLY:
            return "read";
        case SETTING_WRITE_ONLY:
            return "write_only";
        case SETTING_READ_WRITE:
        default:
            return "read_write";
    }
}


static const char *refused_name(const settings_resource_t *resource) {
    switch (resource->refused_while) {
        case SETTINGS_REFUSED_IN_THROW:
            return resource->refuse != NULL ? "in_throw_when_in_use" : "in_throw";
        case SETTINGS_REFUSED_A_JOB_RUNS:
            return "a_job_runs";
        case SETTINGS_REFUSED_NEVER:
        default:
            return resource->refuse != NULL ? "in_throw_when_in_use" : "never";
    }
}


static void write_optional_string(char *out, size_t room, size_t *len, const char *name, const char *value) {
    if (value == NULL) {
        return;
    }
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
    http_json_string(out, room, len, value);
}


static void write_optional_float(char *out, size_t room, size_t *len, const char *name, float value) {
    if (isnan(value)) {
        return;
    }
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
    http_json_float(out, room, len, value);
}


static void write_field_schema(const settings_resource_t *resource, const setting_field_t *field, char *out,
                               size_t room, size_t *len) {
    http_json_raw(out, room, len, "{\"name\":");
    http_json_string(out, room, len, field->name);
    write_optional_string(out, room, len, "label", field->label);
    write_optional_string(out, room, len, "group", field->group);
    write_optional_string(out, room, len, "type", type_name(field));
    write_optional_string(out, room, len, "unit", field->unit);
    // Limits and a step are a number's: a table leaves them 0 for a field of another type
    if (field_is_number(field)) {
        write_optional_float(out, room, len, "min", field->min);
        write_optional_float(out, room, len, "max", field->max);
        if (field->step > 0.0f) {
            write_optional_float(out, room, len, "step", field->step);
        }
        else if (field->type == SETTING_INTEGER || field->held_as_integer) {
            write_optional_float(out, room, len, "step", 1.0f);
        }
    }
    if (field_is_number(field) && field->edit_step > 0.0f) {
        write_optional_float(out, room, len, "edit_step", field->edit_step);
    }
    if (field->short_label != NULL) {
        write_optional_string(out, room, len, "short_label", field->short_label);
    }
    if (field->type == SETTING_BOOLEAN && field->on_label != NULL) {
        write_optional_string(out, room, len, "on_label", field->on_label);
        write_optional_string(out, room, len, "off_label", field->off_label);
    }
    if (field->type == SETTING_STRING) {
        http_json_raw(out, room, len, ",\"min_length\":");
        http_json_integer(out, room, len, field->length_min);
        http_json_raw(out, room, len, ",\"max_length\":");
        http_json_integer(out, room, len, field->size - 1);
    }
    if (field->type == SETTING_CHOICE) {
        http_json_raw(out, room, len, ",\"choices\":[");
        for (uint8_t idx = 0; idx < field->choice_count; idx += 1) {
            if (idx) {
                http_json_raw(out, room, len, ",");
            }
            http_json_raw(out, room, len, "{\"value\":");
            write_choice_value(&field->choices[idx], out, room, len);
            write_optional_string(out, room, len, "label", field->choices[idx].label);
            if (field->choices[idx].short_label != NULL) {
                write_optional_string(out, room, len, "short_label", field->choices[idx].short_label);
            }
            http_json_raw(out, room, len, "}");
        }
        http_json_raw(out, room, len, "]");
    }
    if (field->nullable) {
        http_json_raw(out, room, len, ",\"nullable\":true");
    }
    write_optional_string(out, room, len, "access", access_name(field->access));
    write_optional_string(out, room, len, "applies", field->applies);
    write_optional_string(out, room, len, "refused_while", refused_name(resource));
    write_optional_string(out, room, len, "about", field->about);
    http_json_raw(out, room, len, "}");
}


static const char *const settings_error_codes[] = {
    "bad_json", "bad_request", "length_required", "unknown_field", "wrong_type", "out_of_range", "not_a_choice",
    "read_only", "confirm_missing", "not_found", "method_not_allowed", "busy", "not_running", "no_target",
    "weight_off_zero", "driver_fault", "gate_disabled", "not_supported", "wrong_host", "too_large",
    "wrong_media_type", "failed", "not_an_image", "wrong_chip", "wrong_board", "not_an_update", "not_newer",
    "wrong_slot", "hash_mismatch", "not_ready", "no_network", "not_on_hotspot", "not_now", "not_filling",
    "pin_required", "pin_wrong", "pin_locked",
};


void settings_schema_begin(settings_schema_t *schema, settings_schema_extra_fn extra) {
    memset(schema, 0, sizeof(*schema));
    schema->extra = extra;
}


bool settings_schema_more(void *ctx, char *buf, size_t room, size_t *len) {
    settings_schema_t *schema = ctx;
    *len = 0;
    if (schema->phase == 0) {
        http_json_raw(buf, room, len, "{\"api\":\"1\",");
        write_firmware(buf, room, len);
        http_json_raw(buf, room, len, ",\"resources\":[");
        schema->phase = 1;
        return true;
    }
    if (schema->phase == 1) {
        while (schema->resource < resource_count) {
            const settings_resource_t *resource = resources[schema->resource];
            size_t mark = *len;
            if (schema->field == 0) {
                if (schema->resource > 0) {
                    http_json_raw(buf, room, len, ",");
                }
                http_json_raw(buf, room, len, "{\"path\":");
                http_json_string(buf, room, len, resource->path);
                http_json_raw(buf, room, len, ",\"methods\":[\"GET\",\"PATCH\"]");
                write_optional_string(buf, room, len, "save", resource->save_path);
                write_optional_string(buf, room, len, "revert", resource->revert_path);
                write_optional_string(buf, room, len, "label", resource->label);
                write_optional_string(buf, room, len, "about", resource->about);
                if (resource_has_ids(resource)) {
                    http_json_raw(buf, room, len, ",\"ids\":[");
                    for (int32_t id = 0; id < resource->id_count; id += 1) {
                        if (id) {
                            http_json_raw(buf, room, len, ",");
                        }
                        http_json_integer(buf, room, len, id);
                    }
                    http_json_raw(buf, room, len, "]");
                }
                write_optional_string(buf, room, len, "refused_while", refused_name(resource));
                http_json_raw(buf, room, len, ",\"fields\":[");
            }
            else if (schema->field <= resource->field_count) {
                if (schema->field > 1) {
                    http_json_raw(buf, room, len, ",");
                }
                write_field_schema(resource, &resource->fields[schema->field - 1], buf, room, len);
            }
            else {
                http_json_raw(buf, room, len, "]}");
            }
            if (!stream_item_fits(buf, room, len, mark)) {
                return true;
            }
            if (schema->field <= resource->field_count) {
                schema->field += 1;
            }
            else {
                schema->resource += 1;
                schema->field = 0;
            }
        }
        schema->phase = 2;
        return true;
    }
    if (schema->phase == 2) {
        // The actions: each save with its revert (ADR-008, gate 2), one save a call so a chunk never overflows,
        // then the save of all, then the commands'. field: 0 the head next, 1 none written yet, 2 one written
        if (schema->field == 0) {
            http_json_raw(buf, room, len, "],\"actions\":[");
            schema->resource = 0;
            schema->field = 1;
            return true;
        }
        while (schema->resource < resource_count) {
            size_t idx = schema->resource;
            schema->resource += 1;
            bool done_before = false;
            for (size_t earlier = 0; earlier < idx; earlier += 1) {
                done_before = done_before || strcmp(resources[earlier]->save_path, resources[idx]->save_path) == 0;
            }
            if (done_before) {
                continue;
            }
            size_t mark = *len;
            uint8_t field_before = schema->field;
            http_json_raw(buf, room, len, schema->field == 2 ? "," : "");
            schema->field = 2;
            http_json_raw(buf, room, len, "{\"method\":\"POST\",\"path\":");
            http_json_string(buf, room, len, resources[idx]->save_path);
            http_json_raw(buf, room, len, ",\"fields\":[],\"refused_while\":\"never\","
                                          "\"about\":\"stores what was written there\"},");
            http_json_raw(buf, room, len, "{\"method\":\"POST\",\"path\":");
            http_json_string(buf, room, len, resources[idx]->revert_path);
            http_json_raw(buf, room, len, ",\"fields\":[],\"refused_while\":");
            http_json_string(buf, room, len, resources[idx]->refused_while == SETTINGS_REFUSED_IN_THROW ? "in_throw" :
                                             resources[idx]->refused_while == SETTINGS_REFUSED_A_JOB_RUNS ?
                                             "a_job_runs" : "never");
            http_json_raw(buf, room, len, ",\"about\":\"puts back what is stored there, undoing what was written "
                                          "since: each resource that changed gives its setting changed event. 409 "
                                          "busy as a write is\"}");
            if (!http_json_fits(room, *len) && mark > 0) {
                // As many as fit a chunk: this one begins the next (a chunk each cost the schema 0.3 s)
                *len = mark;
                buf[mark] = '\0';
                schema->resource = (uint8_t) idx;
                schema->field = field_before;
                return true;
            }
        }
        size_t mark = *len;
        http_json_raw(buf, room, len, schema->field == 2 ? "," : "");
        http_json_raw(buf, room, len, "{\"method\":\"POST\",\"path\":\"/api/v1/settings/save\",\"fields\":[],"
                                      "\"refused_while\":\"never\","
                                      "\"about\":\"stores every setting and the powder profiles\"}");
        if (!http_json_fits(room, *len) && mark > 0) {
            *len = mark;
            buf[mark] = '\0';
            return true;    // the save of all begins the next chunk
        }
        schema->phase = 3;
        schema->resource = 0;
        schema->field = 0;
        return true;
    }
    if (schema->phase <= 6) {
        // The commands' actions, the states, the events, the throw record's fields (the API's, handed in at the
        // begin), as many items as fit a chunk
        static const char *const heads[] = {"", "],\"states\":{", "},\"events\":[", "],\"throw\":["};
        while (schema->phase <= 6) {
            size_t part = schema->phase - 3;
            const char *item = schema->extra != NULL ? schema->extra((unsigned) part, schema->resource) : NULL;
            size_t mark = *len;
            if (schema->resource == 0) {
                http_json_raw(buf, room, len, heads[part]);
            }
            if (item != NULL) {
                // The first item of the actions follows the saves'; the first of the others, its part's head
                http_json_raw(buf, room, len, schema->resource > 0 || part == 0 ? "," : "");
                http_json_raw(buf, room, len, item);
            }
            if (!stream_item_fits(buf, room, len, mark)) {
                return true;
            }
            if (item != NULL) {
                schema->resource += 1;
            }
            else {
                schema->phase += 1;
                schema->resource = 0;
            }
        }
        return true;
    }
    http_json_raw(buf, room, len, "],\"errors\":[");
    for (size_t idx = 0; idx < sizeof(settings_error_codes) / sizeof(settings_error_codes[0]); idx += 1) {
        if (idx) {
            http_json_raw(buf, room, len, ",");
        }
        http_json_string(buf, room, len, settings_error_codes[idx]);
    }
    http_json_raw(buf, room, len, "]}");
    return false;
}


// --- The console's `setting`

static const char *skip_spaces(const char *text) {
    while (*text == ' ') {
        text += 1;
    }
    return text;
}


static size_t word_length(const char *text) {
    size_t len = 0;
    while (text[len] != '\0' && text[len] != ' ') {
        len += 1;
    }
    return len;
}


static void console_fields(const settings_resource_t *resource, int32_t id, settings_line_fn write, void *ctx) {
    const uint8_t *base = resource->live(id);
    for (uint8_t idx = 0; idx < resource->field_count; idx += 1) {
        const setting_field_t *field = &resource->fields[idx];
        if (field->access == SETTING_WRITE_ONLY) {
            continue;
        }
        char line[160];
        size_t len = (size_t) snprintf(line, sizeof(line), "@setting %s=", field->name);
        write_value(field, base, line, sizeof(line), &len);
        write(line, ctx);
    }
}


// --- The screen's (ADR-010, gate 4): a field read, and a field written and stored, through the API's own checks

static const settings_resource_t *screen_resource(const char *path, int32_t *id) {
    char full[72];
    bool prefixed = strncmp(path, SETTINGS_API_PREFIX, strlen(SETTINGS_API_PREFIX)) == 0;
    snprintf(full, sizeof(full), "%s%s", prefixed ? "" : SETTINGS_API_PREFIX, path);
    return resource_for_path(full, id);
}


const setting_field_t *settings_field(const char *path, const char *name) {
    int32_t id;
    const settings_resource_t *resource = screen_resource(path, &id);
    if (resource == NULL) {
        return NULL;
    }
    int field_idx = field_index(resource, name, strlen(name));
    return field_idx < 0 ? NULL : &resource->fields[field_idx];
}


bool settings_read_text(const char *path, const char *name, char *out, size_t room) {
    int32_t id;
    const settings_resource_t *resource = screen_resource(path, &id);
    const setting_field_t *field = settings_field(path, name);
    if (resource == NULL || field == NULL || room == 0) {
        return false;
    }
    size_t len = 0;
    out[0] = '\0';
    write_value(field, resource->live(id), out, room, &len);
    // A choice or a string as JSON writes it: its quotes go
    if (len >= 2 && out[0] == '"' && out[len - 1] == '"') {
        memmove(out, out + 1, len - 2);
        out[len - 2] = '\0';
    }
    return true;
}


static char settings_why[128];


const char *settings_write(const char *path, const char *name, const char *value) {
    return settings_write_by(path, name, value, ACTIVITY_BY_BUTTON);     // the menu's
}


const char *settings_write_by(const char *path, const char *name, const char *value, activity_by_t by) {
    int32_t id;
    const settings_resource_t *resource = screen_resource(path, &id);
    if (resource == NULL) {
        snprintf(settings_why, sizeof(settings_why), "no such resource: %s", path);
        return settings_why;
    }
    http_json_member_t member = {.name = name, .name_len = strlen(name), .kind = HTTP_JSON_STRING,
                                 .value = value, .value_len = strlen(value)};
    outcome_t outcome;
    settings_patch(resource, id, &member, 1, true, false, by, &outcome);
    if (outcome.status != 200) {
        snprintf(settings_why, sizeof(settings_why), "%s", outcome.message);
        return settings_why;
    }
    return NULL;
}


// Stored when confirmed, only if it changed (ADR-010, D2)
const char *settings_store(const char *path) {
    return settings_store_by(path, ACTIVITY_BY_BUTTON);      // the menu's
}


const char *settings_store_by(const char *path, activity_by_t by) {
    int32_t id;
    const settings_resource_t *resource = screen_resource(path, &id);
    if (resource == NULL) {
        snprintf(settings_why, sizeof(settings_why), "no such resource: %s", path);
        return settings_why;
    }
    if (!resource->is_saved() && !store(resource, by)) {
        snprintf(settings_why, sizeof(settings_why), "the EEPROM did not take it");
        return settings_why;
    }
    return NULL;
}


const char *settings_write_and_save(const char *path, const char *name, const char *value) {
    const char *why = settings_write(path, name, value);
    return why != NULL ? why : settings_store(path);
}


const char *settings_write_and_store_by(const char *path, const char *const names[], const char *const values[],
                                        size_t count, activity_by_t by) {
    int32_t id;
    const settings_resource_t *resource = screen_resource(path, &id);
    if (resource == NULL || count > SETTINGS_WRITE_BY_MAX) {
        snprintf(settings_why, sizeof(settings_why), "no such resource, or more than %d fields: %s",
                 SETTINGS_WRITE_BY_MAX, path);
        return settings_why;
    }
    // A few fields, on the caller's stack: the Wi-Fi task's (settings_patch takes two copies of the struct beside)
    http_json_member_t members[SETTINGS_WRITE_BY_MAX];
    for (size_t idx = 0; idx < count; idx += 1) {
        members[idx] = (http_json_member_t) {.name = names[idx], .name_len = strlen(names[idx]),
                                             .kind = HTTP_JSON_STRING, .value = values[idx],
                                             .value_len = strlen(values[idx])};
    }
    outcome_t outcome;
    settings_patch(resource, id, members, count, true, false, by, &outcome);
    if (outcome.status != 200) {
        snprintf(settings_why, sizeof(settings_why), "%s", outcome.message);
        return settings_why;
    }
    if (!resource->is_saved() && !store(resource, by)) {
        snprintf(settings_why, sizeof(settings_why), "the EEPROM did not take it");
        return settings_why;
    }
    return NULL;
}


// The boot's copy of the fields that apply at restart, for one resource: walked as settings_restart_pending walks it
void settings_in_effect(const char *path) {
    int32_t id;
    const settings_resource_t *wanted = screen_resource(path, &id);
    size_t used = 0;
    for (size_t idx = 0; idx < resource_count; idx += 1) {
        const settings_resource_t *resource = resources[idx];
        if (resource->id_count != 0) {
            continue;
        }
        const uint8_t *base = resource->live(-1);
        for (uint8_t field_idx = 0; field_idx < resource->field_count; field_idx += 1) {
            const setting_field_t *field = &resource->fields[field_idx];
            if (!applies_at_restart(field) || used + field->size > SETTINGS_BOOT_BYTES) {
                continue;
            }
            if (resource == wanted) {
                memcpy(settings_boot_bytes + used, base + field->offset, field->size);
            }
            used += field->size;
        }
    }
}


void settings_console(const char *args, char *reply, size_t reply_size, settings_line_fn write, void *ctx) {
    args = skip_spaces(args);
    if (*args == '\0') {
        for (size_t idx = 0; idx < resource_count; idx += 1) {
            char line[96];
            snprintf(line, sizeof(line), "@setting %s", resources[idx]->path + strlen(SETTINGS_API_PREFIX));
            write(line, ctx);
        }
        snprintf(reply, reply_size, "@ok setting resources=%u", (unsigned) resource_count);
        return;
    }

    char path[72];
    size_t path_len = word_length(args);
    bool prefixed = strncmp(args, SETTINGS_API_PREFIX, strlen(SETTINGS_API_PREFIX)) == 0;
    snprintf(path, sizeof(path), "%s%.*s", prefixed ? "" : SETTINGS_API_PREFIX, (int) path_len, args);
    int32_t id;
    const settings_resource_t *resource = resource_for_path(path, &id);
    if (resource == NULL) {
        snprintf(reply, reply_size, "@err not_found no such resource: %s", path);
        return;
    }
    args = skip_spaces(args + path_len);

    if (strncmp(args, "save", 4) == 0 && (args[4] == '\0' || args[4] == ' ')) {
        if (!store(resource, ACTIVITY_BY_CONSOLE)) {
            snprintf(reply, reply_size, "@err failed the EEPROM did not take it");
            return;
        }
    }
    else if (strncmp(args, "revert", 6) == 0 && (args[6] == '\0' || args[6] == ' ')) {
        outcome_t outcome;
        revert(resource->revert_path, ACTIVITY_BY_CONSOLE, &outcome);
        if (outcome.status != 200) {
            snprintf(reply, reply_size, "@err %s - %s", outcome.code, outcome.message);
            return;
        }
    }
    else if (*args != '\0') {
        // name=value pairs, as the console types them: each value is read by its field's type
        http_json_member_t members[HTTP_JSON_MEMBERS_MAX];
        size_t count = 0;
        while (*args != '\0' && count < HTTP_JSON_MEMBERS_MAX) {
            size_t pair_len = word_length(args);
            const char *equals = memchr(args, '=', pair_len);
            if (equals == NULL) {
                snprintf(reply, reply_size, "@err bad_request usage: setting <path> [name=value ...|save]");
                return;
            }
            members[count].name = args;
            members[count].name_len = (size_t) (equals - args);
            members[count].kind = HTTP_JSON_STRING;
            members[count].value = equals + 1;
            members[count].value_len = pair_len - members[count].name_len - 1;
            count += 1;
            args = skip_spaces(args + pair_len);
        }
        outcome_t outcome;
        settings_patch(resource, id, members, count, true, true, ACTIVITY_BY_CONSOLE, &outcome);
        if (outcome.status != 200) {
            const char *field_name = outcome.field != NULL ? outcome.field->name :
                                     outcome.field_name[0] ? outcome.field_name : "-";
            snprintf(reply, reply_size, "@err %s %s %s", outcome.code, field_name, outcome.message);
            return;
        }
        // The names it ignored, each as an "@ignored <name>" line, before the fields
        const char *name = outcome.ignored;
        for (uint8_t idx = 0; idx < outcome.ignored_count; idx += 1) {
            char line[80];
            snprintf(line, sizeof(line), "@ignored %s", name);
            write(line, ctx);
            name += strlen(name) + 1;
        }
        if (outcome.ignored_more) {
            write("@ignored ...", ctx);
        }
    }

    console_fields(resource, id, write, ctx);
    snprintf(reply, reply_size, "@ok setting %s saved=%s", path + strlen(SETTINGS_API_PREFIX),
             part_saved(resource, id) ? "true" : "false");
}
