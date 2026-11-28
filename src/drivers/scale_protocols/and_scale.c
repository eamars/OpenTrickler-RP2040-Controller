#include <math.h>
#include <stdlib.h>

#include "scale_make.h"


/*
    A&D FX-i series, standard format: "ST,+00012.34  g" then CR LF, 17 bytes. A frame is taken at its line ending, and
    only a whole one (ADR-012): before, any 17 bytes were a frame, and a byte left over from before, or lost, shifted
    the next frame and it was read as it stood.

    Its other commands, for the record: "PRT" print, "SMP" sample, "U" mode, "P" on and off, "OFF" and "ON" the
    display, each ended by CR LF.
*/
typedef union {
    struct __attribute__((__packed__)){
        char header[2];
        char comma;
        char data[9];
        char unit[3];
        char terminator[2];
    };
    char bytes[17];
} scale_standard_data_format_t;


static float and_scale_parse(const char *frame, size_t len) {
    (void) len;     // the framer checked it
    const scale_standard_data_format_t *msg = (const scale_standard_data_format_t *) frame;
    if (msg->terminator[0] != '\r') {
        return NAN;
    }

    // Decode weight information. The header doesn't really matter
    char *endptr;
    float weight = strtof(msg->data, &endptr);

    if( endptr == msg->data ) {
        // Conversion failed
        return NAN;
    }

    return weight;
}


const scale_make_t and_fxi_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = sizeof(scale_standard_data_format_t),
    .parse = and_scale_parse,
    .zero = "Z\r\n",        // the RE-ZERO key
};
