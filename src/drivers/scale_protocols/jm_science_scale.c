#include <math.h>
#include <stdlib.h>

#include "scale_make.h"


typedef union {
    struct __attribute__((__packed__)){
        char header;
        char _space;
        char stable_state;
        char symbol;
        char weighing_data[9];
        char _space2;
        char weighing_unit[3];
        char terminator[2];
    };
    char bytes[19];
} jm_science_frame_data_format_t;


// A frame starts with its header, and is taken at its length
#define JM_SCIENCE_FRAME_HEADER 'E'


static float jm_science_scale_parse(const char *frame, size_t len) {
    (void) len;
    const jm_science_frame_data_format_t *msg = (const jm_science_frame_data_format_t *) frame;

    // decode sign
    int sign = 1;
    if (msg->symbol == '-') {
        sign = -1;
    }

    // Decode weight information
    char *endptr;
    float weight = strtof(msg->weighing_data, &endptr);

    if( endptr == msg->weighing_data ) {
        // Conversion failed
        return NAN;
    }

    weight *= sign;

    return weight;
}


const scale_make_t jm_science_scale_make = {
    .framing = SCALE_FRAME_AT_HEADER,
    .length = sizeof(jm_science_frame_data_format_t),
    .header = JM_SCIENCE_FRAME_HEADER,
    .parse = jm_science_scale_parse,
};
