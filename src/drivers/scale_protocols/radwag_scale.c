#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "scale_make.h"

/*
    Radwag PS R2 series, in continuous transmission (CU1, set on the scale). Its other commands, each ended by CR LF:
    CU0 continuous off, Z zero, T tare, UT <value> a tare's value, K1 and K0 the keyboard locked and unlocked,
    BP <ms> a beep.
*/

// Radwag response frame structure for SUI command
// Format: SUI<stability><mass(12)><unit(3)>CR LF
// Example: "SUI        1.56 gr \r\n" (stable)
//          "SUI?       2.18 gr \r\n" (unstable)
typedef union {
    struct __attribute__((__packed__)) {
        char command[3];        // "SUI" - immediate reading in current unit
        char stability;         // ' ' stable, '?' unstable, '^' overflow+, 'v' overflow-
        char mass[12];          // 12 chars right-aligned with spaces and dot
        char unit[3];           // 3 chars (usually "gr " with trailing space)
        char terminator[2];     // CR LF
    };
    char bytes[21];
} radwag_sui_frame_t;


/**
 * @brief Decode mass measurement message from Radwag scale
 *
 * @return float Decoded weight value, NaN if conversion failed or the frame is not a SUI mass frame
 */
static float radwag_scale_parse(const char *frame, size_t len) {
    (void) len;
    const radwag_sui_frame_t *msg = (const radwag_sui_frame_t *) frame;

    // Verify this is a SUI mass frame. The stability flag is not checked: an unstable reading is a reading
    if (msg->command[0] != 'S' || msg->command[1] != 'U' || msg->command[2] != 'I') {
        return NAN;
    }

    // Create a temporary buffer for mass conversion
    // Mass field is 12 characters with spaces padding on the left
    char mass_str[13];
    memcpy(mass_str, msg->mass, 12);
    mass_str[12] = '\0';

    // Convert mass string to float (strtof handles leading spaces)
    char *endptr;
    float weight = strtof(mass_str, &endptr);

    if (endptr == mass_str) {
        // Conversion failed
        return NAN;
    }

    // Note: The mass field in SUI doesn't have a separate sign character
    // Negative values would include '-' in the mass field itself

    return weight;
}


const scale_make_t radwag_ps_r2_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .length = sizeof(radwag_sui_frame_t),
    .parse = radwag_scale_parse,
    .zero = "T\r\n",        // a tare, not Z
};
