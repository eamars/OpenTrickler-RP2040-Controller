#include <math.h>
#include <stdlib.h>

#include "scale_make.h"

// Sartorius typically sends data in format like: "+  123.456 g" or similar, a line ended by CR or LF. No zero
// command is known for it: it depends on the model


static float _decode_measurement_msg(const char *msg, size_t len) {
    // Parse Sartorius format:
    // Examples: "     0.000 GN" or "+   27.350" or "+   62.916 GN"
    // Format: optional sign, spaces, decimal number, optional spaces and unit

    int sign = 1;
    size_t start = 0;

    // Check for sign at the beginning
    if (len > 0 && (msg[0] == '-' || msg[0] == '+')) {
        if (msg[0] == '-') {
            sign = -1;
        }
        start = 1;
    }

    // Skip leading spaces
    while (start < len && msg[start] == ' ') {
        start++;
    }

    // Parse the number using strtof which will stop at the first non-numeric character. A frame with no number in it
    // is NaN, as the other drivers say it, not 0.0, which would read as an empty pan (ADR-013)
    if (start >= len) {
        return nanf("");
    }
    char *endptr;
    float value = strtof(&msg[start], &endptr);
    if (endptr == &msg[start]) {
        return nanf("");
    }

    return sign * value;
}


// A line, read without its ending
static float sartorius_scale_parse(const char *frame, size_t len) {
    return _decode_measurement_msg(frame, len - 1);
}


const scale_make_t sartorius_scale_make = {
    .framing = SCALE_FRAME_AT_CR_OR_LF,
    .parse = sartorius_scale_parse,
};
