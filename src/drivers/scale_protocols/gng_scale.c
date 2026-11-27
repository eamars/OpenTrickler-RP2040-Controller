#include <math.h>
#include <stdlib.h>

#include "scale_make.h"

/*
    G&G JJB series: asked for each reading ("p"), and tared to zero ("t").

    Its commands, each ended by CR LF: p print (a reading), q calibrate, r counting, s the weight's unit, t tare,
    u the backlight. The scale's C4 setting (communication) gives the byte before each: ESC (0x1B) as it comes,
    or 33, '!', which the commands here are sent with: set C4 to 33 on the scale.
*/
typedef union {
    struct __attribute__((__packed__)){
        char header[2];
        char data[7];
        char unit[3];
        char terminator[2];
    };
    char bytes[14];
} gngscale_standard_data_format_t;


static float gng_scale_parse(const char *frame, size_t len) {
    (void) len;
    const gngscale_standard_data_format_t *msg = (const gngscale_standard_data_format_t *) frame;

    // Decode header
    // Determine sign
    int sign = 1;

    // Check if the header contains a negative sign
    if (msg->header[0] == '-') {
        sign = -1;
    }

    // Decode weight information
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


const scale_make_t gng_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = sizeof(gngscale_standard_data_format_t),
    .parse = gng_scale_parse,
    .zero = "!t\r\n",
    .request = "!p\r\n",
    .request_ms = 250,
};
