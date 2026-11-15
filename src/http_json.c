#include <math.h>
#include <stdio.h>
#include <string.h>

#include "http_json.h"


static void http_json_put(char *out, size_t room, size_t *len, const char *text, size_t text_len) {
    if (*len >= room) {
        return;
    }
    if (text_len >= room - *len) {
        *len = room;    // full: the reply is refused by whoever checks http_json_fits
        return;
    }
    memcpy(out + *len, text, text_len);
    *len += text_len;
    out[*len] = '\0';
}


void http_json_raw(char *out, size_t room, size_t *len, const char *text) {
    http_json_put(out, room, len, text, strlen(text));
}


void http_json_string(char *out, size_t room, size_t *len, const char *text) {
    http_json_put(out, room, len, "\"", 1);
    for (const unsigned char *s = (const unsigned char *) text; *s; s += 1) {
        char escaped[8];
        if (*s == '"' || *s == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char) *s;
            http_json_put(out, room, len, escaped, 2);
        }
        else if (*s < 0x20) {
            int n = snprintf(escaped, sizeof(escaped), "\\u%04x", *s);
            http_json_put(out, room, len, escaped, (size_t) n);
        }
        else {
            http_json_put(out, room, len, (const char *) s, 1);
        }
    }
    http_json_put(out, room, len, "\"", 1);
}


void http_json_number(char *out, size_t room, size_t *len, float value, int digits) {
    if (!isfinite(value)) {
        http_json_put(out, room, len, "null", 4);
        return;
    }
    char text[24];
    int n = snprintf(text, sizeof(text), "%.*g", digits, (double) value);
    http_json_put(out, room, len, text, (size_t) n);
}


void http_json_integer(char *out, size_t room, size_t *len, long value) {
    char text[16];
    int n = snprintf(text, sizeof(text), "%ld", value);
    http_json_put(out, room, len, text, (size_t) n);
}


void http_json_bool(char *out, size_t room, size_t *len, bool value) {
    http_json_raw(out, room, len, value ? "true" : "false");
}


bool http_json_fits(size_t room, size_t len) {
    return len < room;
}
