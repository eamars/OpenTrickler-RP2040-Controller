#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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


/*
    A float's first digits, correctly rounded: as a whole number of `digits` digits, and the power of ten of its
    first. Not by printf: the Pico SDK's (pico_printf, which stands in for snprintf) writes %.9g of a number below
    0.01 with too few digits, and 40 of 300 floats then did not read back as themselves (ADR-003, gate 3). In doubles:
    a float is exact in one, and the scaling's error is far below the last digit kept.
*/
// Powers of ten a double holds exactly: no pow(), which the RP2040 does in software and slowly
static const double exact_pow10[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22,
};


// magnitude times ten to the power k: a float's range needs k from -54 to 54
static double times_pow10(double magnitude, int k) {
    while (k > 22) {
        magnitude *= 1e22;
        k -= 22;
    }
    while (k < -22) {
        magnitude /= 1e22;
        k += 22;
    }
    return k >= 0 ? magnitude * exact_pow10[k] : magnitude / exact_pow10[-k];
}


static void float_digits(double magnitude, int digits, uint32_t *mantissa, int *exponent) {
    double low = exact_pow10[digits - 1];
    double high = exact_pow10[digits];
    // The power of ten from the power of two (log10(2) is 0.30103), no log10(): it may be one off, put right below
    int binary;
    frexp(magnitude, &binary);
    int e = (int) floor((binary - 1) * 0.30102999566398120);
    double rounded = low;
    for (int tries = 0; tries < 4; tries += 1) {
        rounded = round(times_pow10(magnitude, digits - 1 - e));
        if (rounded < low) {
            e -= 1;
        }
        else if (rounded >= high) {
            e += 1;
        }
        else {
            break;
        }
    }
    *mantissa = (uint32_t) rounded;
    *exponent = e;
}


// A finite float with at most `digits` significant digits (1 to 9), as JSON writes a number; its length
static size_t float_text(float value, int digits, char *out) {
    if (value == 0.0f) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }
    uint32_t mantissa;
    int e;
    float_digits(fabs((double) value), digits, &mantissa, &e);

    char d[12];
    int n = digits;
    for (int idx = digits - 1; idx >= 0; idx -= 1) {
        d[idx] = (char) ('0' + mantissa % 10);
        mantissa /= 10;
    }
    while (n > 1 && d[n - 1] == '0') {
        n -= 1;     // 1.50 is 1.5
    }
    d[n] = '\0';

    size_t pos = 0;
    if (value < 0.0f) {
        out[pos++] = '-';
    }
    if (e >= 0 && e < 9) {
        // 30.02, 1.25, 50, 19200
        for (int idx = 0; idx < n || idx <= e; idx += 1) {
            if (idx == e + 1) {
                out[pos++] = '.';
            }
            out[pos++] = idx < n ? d[idx] : '0';
        }
    }
    else if (e < 0 && e >= -6) {
        // 0.025, 0.00244297716
        out[pos++] = '0';
        out[pos++] = '.';
        for (int idx = -1; idx > e; idx -= 1) {
            out[pos++] = '0';
        }
        memcpy(out + pos, d, (size_t) n);
        pos += (size_t) n;
    }
    else {
        // 3.4e-7, 1e+12
        out[pos++] = d[0];
        if (n > 1) {
            out[pos++] = '.';
            memcpy(out + pos, d + 1, (size_t) n - 1);
            pos += (size_t) n - 1;
        }
        pos += (size_t) snprintf(out + pos, 8, "e%d", e);
    }
    out[pos] = '\0';
    return pos;
}


void http_json_number(char *out, size_t room, size_t *len, float value, int digits) {
    if (!isfinite(value)) {
        http_json_put(out, room, len, "null", 4);
        return;
    }
    char text[32];
    size_t n = float_text(value, digits < 1 ? 1 : digits > 9 ? 9 : digits, text);
    http_json_put(out, room, len, text, n);
}


void http_json_float(char *out, size_t room, size_t *len, float value) {
    if (!isfinite(value)) {
        http_json_put(out, room, len, "null", 4);
        return;
    }
    // A float needs nine significant digits at most to read back as itself; most settings need far fewer (30.02)
    char text[32];
    size_t n = 0;
    for (int digits = 1; digits <= 9; digits += 1) {
        n = float_text(value, digits, text);
        if (strtof(text, NULL) == value) {
            break;
        }
    }
    http_json_put(out, room, len, text, n);
}


void http_json_integer(char *out, size_t room, size_t *len, long value) {
    char text[16];
    int n = snprintf(text, sizeof(text), "%ld", value);
    http_json_put(out, room, len, text, (size_t) n);
}


void http_json_unsigned(char *out, size_t room, size_t *len, uint32_t value) {
    char text[16];
    int n = snprintf(text, sizeof(text), "%lu", (unsigned long) value);
    http_json_put(out, room, len, text, (size_t) n);
}


void http_json_u64(char *out, size_t room, size_t *len, uint64_t value) {
    char digits[24];
    size_t pos = sizeof(digits) - 1;
    digits[pos] = '\0';
    do {
        digits[--pos] = (char) ('0' + value % 10);
        value /= 10;
    } while (value > 0 && pos > 0);
    http_json_raw(out, room, len, digits + pos);
}


void http_json_member(char *out, size_t room, size_t *len, const char *name) {
    http_json_raw(out, room, len, ",\"");
    http_json_raw(out, room, len, name);
    http_json_raw(out, room, len, "\":");
}


void http_json_bool(char *out, size_t room, size_t *len, bool value) {
    http_json_raw(out, room, len, value ? "true" : "false");
}


bool http_json_fits(size_t room, size_t len) {
    return len < room;
}


// The reading of a flat object: where it stands in the text
typedef struct {
    const char *text;
    size_t len;
    size_t pos;
} json_reader_t;


static void json_skip_space(json_reader_t *reader) {
    while (reader->pos < reader->len) {
        char c = reader->text[reader->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return;
        }
        reader->pos += 1;
    }
}


static bool json_take(json_reader_t *reader, char c) {
    json_skip_space(reader);
    if (reader->pos < reader->len && reader->text[reader->pos] == c) {
        reader->pos += 1;
        return true;
    }
    return false;
}


static bool json_is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}


// A string at the reader, its quotes passed: its inside, escapes checked but not decoded
static bool json_read_string(json_reader_t *reader, const char **start, size_t *len) {
    if (!json_take(reader, '"')) {
        return false;
    }
    *start = reader->text + reader->pos;
    while (reader->pos < reader->len) {
        unsigned char c = (unsigned char) reader->text[reader->pos];
        if (c == '"') {
            *len = (size_t) (reader->text + reader->pos - *start);
            reader->pos += 1;
            return true;
        }
        if (c < 0x20) {
            return false;       // a control character must be escaped
        }
        if (c == '\\') {
            if (reader->pos + 1 >= reader->len) {
                return false;
            }
            char escaped = reader->text[reader->pos + 1];
            if (escaped == 'u') {
                if (reader->pos + 5 >= reader->len) {
                    return false;
                }
                for (int idx = 2; idx <= 5; idx += 1) {
                    if (!json_is_hex(reader->text[reader->pos + idx])) {
                        return false;
                    }
                }
                reader->pos += 6;
                continue;
            }
            if (escaped == '\0' || strchr("\"\\/bfnrt", escaped) == NULL) {
                return false;
            }
            reader->pos += 2;
            continue;
        }
        reader->pos += 1;
    }
    return false;
}


static bool json_is_digit(char c) {
    return c >= '0' && c <= '9';
}


// A number by JSON's grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
static bool json_read_number(json_reader_t *reader, const char **start, size_t *len) {
    json_skip_space(reader);
    const char *t = reader->text;
    size_t pos = reader->pos;
    size_t end = reader->len;
    *start = t + pos;
    if (pos < end && t[pos] == '-') {
        pos += 1;
    }
    if (pos >= end || !json_is_digit(t[pos])) {
        return false;
    }
    if (t[pos] == '0') {
        pos += 1;
    }
    else {
        while (pos < end && json_is_digit(t[pos])) {
            pos += 1;
        }
    }
    if (pos < end && t[pos] == '.') {
        pos += 1;
        if (pos >= end || !json_is_digit(t[pos])) {
            return false;
        }
        while (pos < end && json_is_digit(t[pos])) {
            pos += 1;
        }
    }
    if (pos < end && (t[pos] == 'e' || t[pos] == 'E')) {
        pos += 1;
        if (pos < end && (t[pos] == '+' || t[pos] == '-')) {
            pos += 1;
        }
        if (pos >= end || !json_is_digit(t[pos])) {
            return false;
        }
        while (pos < end && json_is_digit(t[pos])) {
            pos += 1;
        }
    }
    *len = (size_t) (t + pos - *start);
    reader->pos = pos;
    return true;
}


static bool json_read_word(json_reader_t *reader, const char *word, const char **start, size_t *len) {
    json_skip_space(reader);
    size_t word_len = strlen(word);
    if (reader->len - reader->pos < word_len || memcmp(reader->text + reader->pos, word, word_len) != 0) {
        return false;
    }
    *start = reader->text + reader->pos;
    *len = word_len;
    reader->pos += word_len;
    return true;
}


bool http_json_parse_object(const char *text, size_t len, http_json_member_t *members, size_t max, size_t *count,
                            const char **why) {
    json_reader_t reader = {.text = text, .len = len, .pos = 0};
    *count = 0;
    *why = "not a JSON object";
    if (!json_take(&reader, '{')) {
        return false;
    }
    if (!json_take(&reader, '}')) {
        while (true) {
            if (*count >= max) {
                *why = "too many fields";
                return false;
            }
            http_json_member_t *member = &members[*count];
            if (!json_read_string(&reader, &member->name, &member->name_len) || !json_take(&reader, ':')) {
                return false;
            }
            json_skip_space(&reader);
            char c = reader.pos < reader.len ? text[reader.pos] : '\0';
            bool ok;
            if (c == '"') {
                member->kind = HTTP_JSON_STRING;
                ok = json_read_string(&reader, &member->value, &member->value_len);
            }
            else if (c == 't' || c == 'f') {
                member->kind = HTTP_JSON_BOOLEAN;
                ok = json_read_word(&reader, c == 't' ? "true" : "false", &member->value, &member->value_len);
            }
            else if (c == 'n') {
                member->kind = HTTP_JSON_NULL;
                ok = json_read_word(&reader, "null", &member->value, &member->value_len);
            }
            else if (c == '{' || c == '[') {
                *why = "a field is an object or a list: the API's writes are flat";
                return false;
            }
            else {
                member->kind = HTTP_JSON_NUMBER;
                ok = json_read_number(&reader, &member->value, &member->value_len);
            }
            if (!ok) {
                return false;
            }
            for (size_t idx = 0; idx < *count; idx += 1) {
                if (members[idx].name_len == member->name_len &&
                    memcmp(members[idx].name, member->name, member->name_len) == 0) {
                    *why = "a field is given twice";
                    return false;
                }
            }
            *count += 1;
            if (json_take(&reader, '}')) {
                break;
            }
            if (!json_take(&reader, ',')) {
                return false;
            }
        }
    }
    json_skip_space(&reader);
    if (reader.pos != reader.len) {
        *why = "more after the object";
        return false;
    }
    return true;
}


bool http_json_is_number(const char *text, size_t len) {
    json_reader_t reader = {.text = text, .len = len, .pos = 0};
    const char *start;
    size_t number_len;
    return json_read_number(&reader, &start, &number_len) && reader.pos == len && text[0] != ' ';
}


bool http_json_name_is(const http_json_member_t *member, const char *name) {
    size_t name_len = strlen(name);
    return member->name_len == name_len && memcmp(member->name, name, name_len) == 0;
}


static unsigned json_hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return (unsigned) (c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (unsigned) (c - 'a' + 10);
    }
    return (unsigned) (c - 'A' + 10);
}


bool http_json_string_value(const http_json_member_t *member, char *out, size_t room) {
    size_t used = 0;
    const char *s = member->value;
    const char *end = member->value + member->value_len;
    while (s < end) {
        char decoded[4];
        size_t decoded_len = 1;
        if (*s != '\\') {
            decoded[0] = *s;
            s += 1;
        }
        else if (s[1] == 'u') {
            // Checked when parsed: four hex digits. A code point of the Basic Multilingual Plane as UTF-8; a
            // surrogate, half of a pair, is not taken
            unsigned code = (json_hex_value(s[2]) << 12) | (json_hex_value(s[3]) << 8) |
                            (json_hex_value(s[4]) << 4) | json_hex_value(s[5]);
            s += 6;
            if (code == 0 || (code >= 0xd800 && code <= 0xdfff)) {
                return false;
            }
            if (code < 0x80) {
                decoded[0] = (char) code;
            }
            else if (code < 0x800) {
                decoded[0] = (char) (0xc0 | (code >> 6));
                decoded[1] = (char) (0x80 | (code & 0x3f));
                decoded_len = 2;
            }
            else {
                decoded[0] = (char) (0xe0 | (code >> 12));
                decoded[1] = (char) (0x80 | ((code >> 6) & 0x3f));
                decoded[2] = (char) (0x80 | (code & 0x3f));
                decoded_len = 3;
            }
        }
        else {
            // Checked when parsed: one of " \ / b f n r t
            switch (s[1]) {
                case 'b': decoded[0] = '\b'; break;
                case 'f': decoded[0] = '\f'; break;
                case 'n': decoded[0] = '\n'; break;
                case 'r': decoded[0] = '\r'; break;
                case 't': decoded[0] = '\t'; break;
                default:  decoded[0] = s[1]; break;
            }
            s += 2;
        }
        if (used + decoded_len >= room) {
            return false;
        }
        memcpy(out + used, decoded, decoded_len);
        used += decoded_len;
    }
    out[used] = '\0';
    return true;
}


bool http_json_number_value(const http_json_member_t *member, double *value) {
    char text[40];
    if (member->kind != HTTP_JSON_NUMBER || member->value_len >= sizeof(text)) {
        return false;
    }
    memcpy(text, member->value, member->value_len);
    text[member->value_len] = '\0';
    *value = strtod(text, NULL);
    return isfinite(*value);
}
