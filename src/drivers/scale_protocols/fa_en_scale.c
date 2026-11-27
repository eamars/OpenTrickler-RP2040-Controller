#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <math.h>
#include <task.h>
#include <string.h>

#include "chip_serial.h"
#include "configuration.h"
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
  by fixed offsets.
*/

#define FA_EN_FRAME_BUFFER_SIZE         32

// Forward declaration
void _fa_en_scale_listener_task(void *p);
static void force_zero();

// Instance of the scale handle for FA-E / FA-EN series
scale_handle_t fa_en_scale_handle = {
    .read_loop_task = _fa_en_scale_listener_task,
    .force_zero = force_zero,
};

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

void _fa_en_scale_listener_task(void *p) {
    char buf[FA_EN_FRAME_BUFFER_SIZE];
    uint8_t buf_idx = 0;

    while (true) {
        // Read all available data
        int got;
        while ((got = chip_serial_read(SCALE_UART)) != CHIP_SERIAL_NONE) {
            char ch = (char) got;

            if (ch == '\r' || ch == '\n') {
                // The balance ends a frame with CR (the manual lists two). Empty lines are skipped.
                if (buf_idx > 0) {
                    buf[buf_idx] = '\0';

                    // A line that doesn't start with 'E' is the tail of a partial frame: drop it
                    if (buf[0] == 'E') {
                        // Publish it; a frame with no number (NaN) is dropped there and counted (ADR-013)
                        scale_make_host->publish_measurement(_decode_measurement_msg(buf));
                    }

                    buf_idx = 0;
                }
            }
            else if (buf_idx < sizeof(buf) - 1) {
                buf[buf_idx++] = ch;
            }
            else {
                // Overflow, wait for the next frame
                buf_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void force_zero() {
    // The manual lists no command to zero the balance over the serial port
}
