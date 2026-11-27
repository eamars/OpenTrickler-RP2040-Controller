#include <math.h>
#include <stdlib.h>

#include "scale_make.h"

/*
  Example data
    +   20.758g
    +   20.758g
    +   20.758g
    +  320.344GN
    +  320.344GN
    +  320.344GN
    +  320.344GN
    +  889.629GN
    + 1116.438GN
    + 1320.253GN
    + 1508.019GN
    + ~~~~~~~~GN
*/
typedef union {
    struct __attribute__((__packed__)) {
        char header[2];         // + or -
        char data[8];           // Float integer
        char unit[3];           // GN (or something else)
        char terminator[2];     // \r\n (carriage return and newline)
    };
    char bytes[15];
} ussolid_jfdbs_data_format_t ;


static float ussolid_scale_parse(const char *frame, size_t len) {
    (void) len;
    const ussolid_jfdbs_data_format_t *msg = (const ussolid_jfdbs_data_format_t *) frame;

    // Determine sign
    int sign = 1;

    // Check if the header contains a negative sign
    if (msg->header[0] == '-') {
        sign = -1;
    }

    char *endptr;
    float weight = strtof(msg->data, &endptr);

    if( endptr == msg->data ) {
        // Conversion failed
        return NAN;
    }

    // Apply the sign
    weight *= sign;

    return weight;
}


const scale_make_t ussolid_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = sizeof(ussolid_jfdbs_data_format_t),
    .parse = ussolid_scale_parse,
};
