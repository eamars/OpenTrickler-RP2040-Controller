#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "scale_make.h"

/*
  FA-E / FA-EN series analytical balance (aelabgroup.com), continuous output.

  Frame (user manual 3.5), terminated by carriage return:
    'E' ' ' <stable> <sign> <weight, decimal point included> ' ' <unit> CR
      stable: '*' unstable, ' ' stable
      sign:   '+', '-' or ' '

  Example data (4800 baud, 8N1; the manual's default is 1200, set the balance to
  4800 or faster from its BAUD menu):
    E     0.0000  g
    E *+  0.0238  g
    E *-  5.4205  g
    E *+ 46.1366  g

  The field widths in the manual and in a captured stream differ by a character,
  so the frame is read as a line and the number is found by its characters, not
  by fixed offsets. The manual lists no command to zero the balance over the serial port.
*/

static float _decode_measurement_msg(const char *msg) {
    // Every frame starts with 'E'. Anything else is noise from a partial frame.
    if (msg[0] != 'E') {
        return NAN;
    }

    // Skip the header, the space and the stable flag. The sign comes before the first digit.
    const char *p = msg + 1;
    bool negative = false;
    while (*p && !((*p >= '0' && *p <= '9') || *p == '.')) {
        if (*p == '-') {
            negative = true;
        }
        p++;
    }

    if (*p == '\0') {
        // No number in the frame (for example an error message)
        return NAN;
    }

    char *endptr;
    float weight = strtof(p, &endptr);
    if (endptr == p) {
        return NAN;
    }

    return negative ? -weight : weight;
}


// A line ended by CR (the manual lists two endings; an empty line between is no frame)
static float fa_en_scale_parse(const char *frame, size_t len) {
    (void) len;
    return _decode_measurement_msg(frame);
}


const scale_make_t fa_en_scale_make = {
    .framing = SCALE_FRAME_AT_CR_OR_LF,
    .parse = fa_en_scale_parse,
};
