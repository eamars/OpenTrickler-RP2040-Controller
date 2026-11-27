#include <math.h>
#include <stdlib.h>

#include "scale_make.h"

/*
  Example data
    S       0.00 GN
    S       0.00 GN
    SD     -1.14 GN
    SD   -143.02 GN
    SD   -467.16 GN

  A frame is the example's 17 bytes, 15 characters and CR LF (ADR-017, gate 2). The fields below miss the space
  before the unit; the number lies where they put it.
*/
typedef union {
    struct __attribute__((__packed__)) {
        char header[2];         // S or SD
        char data[10];          // Signed integer
        char unit[2];           // GN (or something else)
        char terminator[2];     // \r\n (carriage return)
    };
    char bytes[16];
} steinberg_sbs_data_format_t ;

#define STEINBERG_FRAME_LENGTH 17


static float steinberg_scale_parse(const char *frame, size_t len) {
    (void) len;
    const steinberg_sbs_data_format_t *msg = (const steinberg_sbs_data_format_t *) frame;

    // Decode weight information
    char *endptr;
    float weight = strtof(msg->data, &endptr);

    if( endptr == msg->data ) {
        // Conversion failed
        return NAN;
    }

    return weight;
}


const scale_make_t steinberg_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = STEINBERG_FRAME_LENGTH,
    .parse = steinberg_scale_parse,
};
