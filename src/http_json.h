#ifndef HTTP_JSON_H_
#define HTTP_JSON_H_
/*
 * JSON written for the API (ADR-003): strings escaped, numbers that are not numbers written as null. Each call
 * appends to a buffer and never past its end: *len reaches room when the buffer is full.
 */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Text as it is
void http_json_raw(char *out, size_t room, size_t *len, const char *text);
// A string, quoted and escaped
void http_json_string(char *out, size_t room, size_t *len, const char *text);
// A number with at most this many significant digits; null when it is not finite
void http_json_number(char *out, size_t room, size_t *len, float value, int digits);
void http_json_integer(char *out, size_t room, size_t *len, long value);
void http_json_bool(char *out, size_t room, size_t *len, bool value);
// True when the buffer was not filled
bool http_json_fits(size_t room, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // HTTP_JSON_H_
