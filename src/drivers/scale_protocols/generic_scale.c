#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "scale_make.h"


/*
    The Generic make: a line ended by LF, its first number its weight. It knows no zero command for its scale: asked
    to zero, it does nothing (ADR-012; audit S1).
*/


/*
    A frame's weight: its first number, past any prefix ("ST", "GS,"), from a digit or a point before one, with the
    sign before it, spaces between allowed ("- 12.34", "+ 0.50"). False when the frame holds no number
*/
static bool _generic_scale_parse(const char *frame, float *weight) {
    const char *sign = NULL;
    for (const char *at = frame; *at != '\0'; at += 1) {
        if (*at == '-' || *at == '+') {
            sign = at;
            continue;
        }
        if (*at == ' ' && sign != NULL) {
            continue;
        }
        if (isdigit((unsigned char) at[0]) || (at[0] == '.' && isdigit((unsigned char) at[1]))) {
            float value = strtof(at, NULL);
            *weight = sign != NULL && *sign == '-' ? -value : value;
            return true;
        }
        sign = NULL;
    }
    return false;
}


static float generic_scale_frame_parse(const char *frame, size_t len) {
    (void) len;
    float weight;
    return _generic_scale_parse(frame, &weight) ? weight : NAN;
}


const scale_make_t generic_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .parse = generic_scale_frame_parse,
};
