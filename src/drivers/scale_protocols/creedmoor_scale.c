#include <math.h>
#include <stdlib.h>

#include "scale_make.h"

/*
Example data
-0000.00 GN \r\n
+0000.00 GN \r\n
+0142.02 GN \r\n
+0.32445 oz \r\n
+045.991 ct \r\n
+0.02027 lb \r\n
+009.198 g  \r\n
*/

typedef union {
    struct __attribute__((__packed__)) {
        char header[1];         // Sign (+ or -)
        char data[7];           // Unsigned integer with leading zeros
        char _space;            // Space
        char unit[2];           // Unit GN (or something else)
        char _space2;           // Space
        char terminator[2];     // \r\n (carriage return)
    };
    char bytes[14];
} creedmoor_data_format_t ;


static float creedmoor_scale_parse(const char *frame, size_t len) {
    (void) len;
    const creedmoor_data_format_t *msg = (const creedmoor_data_format_t *) frame;

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


const scale_make_t creedmoor_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = sizeof(creedmoor_data_format_t),
    .parse = creedmoor_scale_parse,
};
