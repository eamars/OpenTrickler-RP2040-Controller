#ifndef HTTP_JSON_H_
#define HTTP_JSON_H_
/*
 * JSON written for the API (ADR-003): strings escaped, numbers that are not numbers written as null. Each call
 * appends to a buffer and never past its end: *len reaches room when the buffer is full.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Text as it is
void http_json_raw(char *out, size_t room, size_t *len, const char *text);
// A string, quoted and escaped
void http_json_string(char *out, size_t room, size_t *len, const char *text);
// A number with at most this many significant digits; null when it is not finite
void http_json_number(char *out, size_t room, size_t *len, float value, int digits);
// A setting's float: the shortest decimal that reads back to the same float, nine significant digits at most
// (api_v1.md: a number a GET gives, PATCHed back, changes nothing); null when it is not finite
void http_json_float(char *out, size_t room, size_t *len, float value);
void http_json_integer(char *out, size_t room, size_t *len, long value);
// A 32-bit number with no sign (the boot's id): a long is 32 bits here, and would print the top half negative
void http_json_unsigned(char *out, size_t room, size_t *len, uint32_t value);
void http_json_bool(char *out, size_t room, size_t *len, bool value);
// True when the buffer was not filled
bool http_json_fits(size_t room, size_t len);

/*
 * JSON read (ADR-003, gate 3): a flat object, as every write of the API is. Its members' values are strings,
 * numbers, true, false or null; and, since ADR-009's gate R, a list of rows, each a flat object (one level only: a
 * calibration's stops). The members point into the text: nothing is copied or decoded until
 * a value is asked for.
 */
typedef enum {
    HTTP_JSON_STRING,
    HTTP_JSON_NUMBER,
    HTTP_JSON_BOOLEAN,
    HTTP_JSON_NULL,
    HTTP_JSON_ARRAY,        // a list of flat objects, its text whole from [ to ]: read its rows with http_json_array_next
} http_json_kind_t;

typedef struct {
    const char *name;       // the name's text, its escapes as sent, not ended
    size_t name_len;
    http_json_kind_t kind;
    const char *value;      // a string's inside as sent, a number's text, "true", "false" or "null"; not ended
    size_t value_len;
} http_json_member_t;

#define HTTP_JSON_MEMBERS_MAX 32

/*
 * Parse a flat object. False, with why, when the text is not one, has more than max members, or gives a name twice.
 */
bool http_json_parse_object(const char *text, size_t len, http_json_member_t *members, size_t max, size_t *count,
                            const char **why);
// A list's next row: its object's text, for http_json_parse_object. `at` starts at 0. False after the last
bool http_json_array_next(const http_json_member_t *array, size_t *at, const char **object, size_t *object_len);
// Whether the text is one number by JSON's grammar, and nothing else (the console's values)
bool http_json_is_number(const char *text, size_t len);
// Whether a member's name is this one (a name with an escape is no name of ours)
bool http_json_name_is(const http_json_member_t *member, const char *name);
// A string member's value, its escapes decoded (\u as UTF-8), ended. False when it does not fit in room
bool http_json_string_value(const http_json_member_t *member, char *out, size_t room);
// A number member's value; false when it is not a finite double
bool http_json_number_value(const http_json_member_t *member, double *value);

#ifdef __cplusplus
}
#endif

#endif  // HTTP_JSON_H_
