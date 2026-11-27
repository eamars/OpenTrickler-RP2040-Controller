#ifndef SCALE_MAKE_H_
#define SCALE_MAKE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
    A make of scale (ADR-017, gate 2): how one maker's frames are found on the serial port and read, and what its
    scale is sent. A make is a table entry and a parser, with no task and no state of its own: one reader, the scale's
    (services/scale), reads the port for whichever make runs, finds each frame as the make says (scale_framer_take())
    and hands it to the make's parser. A make names nothing above itself: the start-up's wiring hands each to the
    scale by its stored number (scale_register_make(), scale.h). A build may carry any of them.
*/

// How a make's frame is found
typedef enum {
    SCALE_FRAME_AT_LF = 0,          // a line, ended by LF
    SCALE_FRAME_AT_CR_OR_LF,        // a line, ended by CR or by LF
    SCALE_FRAME_AT_HEADER,          // from its header byte, taken at its length
} scale_framing_t;

// The most bytes a frame holds, its line ending included. A longer line is dropped whole, as a frame with no number
#define SCALE_FRAME_MOST 32

typedef struct {
    scale_framing_t framing;
    // A frame's bytes, its line ending included: a line of another length is a frame with no number. 0, a line of
    // any length up to SCALE_FRAME_MOST
    uint8_t length;
    char header;                    // SCALE_FRAME_AT_HEADER: the byte a frame starts with
    // The weight a frame reads as, in the scale's unit; NAN for a frame with no number. `frame` is its `len` bytes
    // as they came, its line ending included, with a NUL after them
    float (*parse)(const char *frame, size_t len);
    // Where in a frame the unit of its weight lies, for the scale to report (the A&D's); unit_len 0 for none
    uint8_t unit_at;
    uint8_t unit_len;
    const char *zero;               // the command that zeroes the scale; NULL, it cannot be zeroed from here
    const char *request;            // asked for: the request each reading answers, sent every request_ms; NULL, it streams
    uint32_t request_ms;
} scale_make_t;

// The reader's state while it finds one make's frames
typedef struct {
    const scale_make_t *make;
    char frame[SCALE_FRAME_MOST + 1];
    uint8_t len;                    // bytes held
    bool overlong;                  // the line ran past SCALE_FRAME_MOST: dropped whole at its end
    bool seeking;                   // nothing is a frame before the first frame's start
} scale_framer_t;

/*
    What is done with each frame found: its bytes as they came (a line too long, its first SCALE_FRAME_MOST) and the
    weight they read as, NAN for a frame with no number: a line of the wrong length, too long, a frame cut short by
    the next one's header, or one its parser read no number in
*/
typedef void (*scale_frame_sink_t)(void *context, const char *frame, size_t len, float weight);

#ifdef __cplusplus
extern "C" {
#endif

// A framer started afresh, at the start and after the port was drained: what comes before the first frame's start
// (a line ending, or the header byte) is the end of a frame already under way, and no frame
void scale_framer_start(scale_framer_t *framer, const scale_make_t *make);

// A byte from the port; each frame it ends goes to the sink, at once
void scale_framer_take(scale_framer_t *framer, uint8_t byte, scale_frame_sink_t sink, void *context);

// The makes
extern const scale_make_t and_fxi_scale_make;
extern const scale_make_t steinberg_scale_make;
extern const scale_make_t gng_scale_make;
extern const scale_make_t ussolid_scale_make;
extern const scale_make_t jm_science_scale_make;
extern const scale_make_t creedmoor_scale_make;
extern const scale_make_t radwag_ps_r2_scale_make;
extern const scale_make_t sartorius_scale_make;
extern const scale_make_t generic_scale_make;
extern const scale_make_t fa_en_scale_make;

#ifdef __cplusplus
}
#endif

#endif  // SCALE_MAKE_H_
