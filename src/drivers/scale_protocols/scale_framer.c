/*
    A make's frames found in the bytes from the port (scale_make.h, ADR-017 gate 2): a frame is taken whole, from its
    start to its end, and checked for its length before its parser reads it.
*/
#include <math.h>

#include "scale_make.h"


void scale_framer_start(scale_framer_t *framer, const scale_make_t *make) {
    framer->make = make;
    framer->len = 0;
    framer->overlong = false;
    framer->seeking = true;
}


// The frame held, handed on: read by the make's parser when whole, else a frame with no number
static void scale_framer_hand(scale_framer_t *framer, bool whole, scale_frame_sink_t sink, void *context) {
    framer->frame[framer->len] = '\0';
    float weight = whole ? framer->make->parse(framer->frame, framer->len) : NAN;
    sink(context, framer->frame, framer->len, weight);
}


static bool scale_framer_is_blank(const scale_framer_t *framer) {
    for (uint8_t idx = 0; idx < framer->len; idx += 1) {
        if (framer->frame[idx] != '\r' && framer->frame[idx] != '\n') {
            return false;
        }
    }
    return true;
}


// A frame from its header byte, taken at its length. What follows it until the next header is no frame
static void scale_framer_take_from_header(scale_framer_t *framer, uint8_t byte, scale_frame_sink_t sink,
                                          void *context) {
    const scale_make_t *make = framer->make;

    if (byte == (uint8_t) make->header) {
        if (!framer->seeking && framer->len > 0) {
            scale_framer_hand(framer, false, sink, context);    // cut short by the next frame
        }
        framer->seeking = false;
        framer->len = 0;
    }
    if (framer->seeking) {
        return;
    }

    framer->frame[framer->len++] = (char) byte;
    if (framer->len >= make->length || framer->len >= SCALE_FRAME_MOST) {
        scale_framer_hand(framer, true, sink, context);
        framer->len = 0;
        framer->seeking = true;
    }
}


// A line: from one line ending to the next, its length checked. A line holding only CR and LF is no frame
static void scale_framer_take_line(scale_framer_t *framer, uint8_t byte, scale_frame_sink_t sink, void *context) {
    const scale_make_t *make = framer->make;
    bool ends = byte == '\n' || (byte == '\r' && make->framing == SCALE_FRAME_AT_CR_OR_LF);

    if (framer->seeking) {
        framer->seeking = !ends;
        return;
    }

    if (framer->len < SCALE_FRAME_MOST) {
        framer->frame[framer->len++] = (char) byte;
    }
    else {
        framer->overlong = true;
    }
    if (!ends) {
        return;
    }

    if (framer->overlong || !scale_framer_is_blank(framer)) {
        bool whole = !framer->overlong && (make->length == 0 || framer->len == make->length);
        scale_framer_hand(framer, whole, sink, context);
    }
    framer->len = 0;
    framer->overlong = false;
}


void scale_framer_take(scale_framer_t *framer, uint8_t byte, scale_frame_sink_t sink, void *context) {
    if (framer->make->framing == SCALE_FRAME_AT_HEADER) {
        scale_framer_take_from_header(framer, byte, sink, context);
    }
    else {
        scale_framer_take_line(framer, byte, sink, context);
    }
}
