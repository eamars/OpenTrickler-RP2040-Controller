/*
    The scale's one reader (ADR-017, gate 2): every make is read here, as its table entry says (scale_make.h). It
    sleeps until a byte comes (chip_serial_read_within), so a frame is published as it ends, not at the next poll.
*/
#include <math.h>
#include <string.h>

#include "chip.h"
#include "chip_serial.h"
#include "configuration.h"
#include "scale.h"
#include "scale_reader.h"

// The longest wait for a byte when nothing is to be asked of the scale: the reader then only waits again
#define SCALE_READER_WAIT_US 1000000u

// Asked from another task while the reader stands suspended; taken by the reader
static volatile bool scale_reader_restart_asked = false;


void scale_reader_restart(void) {
    scale_reader_restart_asked = true;
}


// A frame found: tapped as it came (ADR-013), and published through the one door, NAN for one with no number
static void scale_reader_publish(void *context, const char *frame, size_t len, float weight) {
    (void) context;
    scale_tap_frame((const uint8_t *) frame, len, weight);
    scale_publish_measurement(weight);
}


// The framer started afresh on an empty port. What the port holds was sent while nothing read it (the start, before
// the scheduler runs; a stand-in's time, the reader suspended): its FIFO filled and the bytes after it were lost, so
// its last line would be joined to a later one
static void scale_reader_start_afresh(scale_framer_t *framer, const scale_make_t *make) {
    while (chip_serial_read(SCALE_UART) != CHIP_SERIAL_NONE) {
    }
    scale_framer_start(framer, make);
}


void scale_reader_task(void *p) {
    const scale_make_t *make = p;
    scale_framer_t framer;
    scale_reader_start_afresh(&framer, make);
    uint64_t next_request_us = chip_time_us();

    while (true) {
        uint32_t wait_us = SCALE_READER_WAIT_US;
        if (make->request != NULL) {
            uint64_t now_us = chip_time_us();
            if (now_us >= next_request_us) {
                scale_write(make->request, strlen(make->request));
                next_request_us = now_us + (uint64_t) make->request_ms * 1000u;
            }
            wait_us = (uint32_t) (next_request_us - now_us);
        }

        int got = chip_serial_read_within(SCALE_UART, wait_us);
        if (scale_reader_restart_asked) {
            scale_reader_restart_asked = false;
            scale_reader_start_afresh(&framer, make);
            continue;    // the byte taken was the stale port's
        }
        if (got != CHIP_SERIAL_NONE) {
            scale_framer_take(&framer, (uint8_t) got, scale_reader_publish, (void *) make);
        }
    }
}
