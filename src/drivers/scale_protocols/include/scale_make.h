#ifndef SCALE_MAKE_H_
#define SCALE_MAKE_H_

#include <stddef.h>
#include <stdint.h>

/*
    A make of scale (ADR-006, gate 6): how one maker's scale is read on the serial port and told to zero. Each make
    is a task that reads frames and hands each reading to its host, the scale's service above it, which runs one make
    at a time. A make names nothing above itself: its host is handed to the makes (scale_makes_set_host()), and the
    start-up's wiring hands each make to the scale by its stored number (scale_register_make(), scale.h). A build
    may carry any of them.
*/

// A make: its reading task, and its zero
typedef struct {
    void (*read_loop_task)(void *self);
    void (*force_zero)(void);
} scale_handle_t;

// What a make is given by whoever runs it
typedef struct {
    // A reading, in the scale's unit; NAN for a frame that did not parse
    void (*publish_measurement)(float weight);
    // The unit a frame carried, where the make's frames carry one
    void (*publish_unit)(const char *unit, size_t len);
    // A frame as it came and what it read as, for the tap (ADR-013)
    void (*tap_frame)(const uint8_t *bytes, size_t len, float value);
    // Bytes to the scale, under the port's lock
    void (*write)(const char *command, size_t len);
} scale_make_host_t;

#ifdef __cplusplus
extern "C" {
#endif

// The host, before any make's task runs. The struct is kept by its address
void scale_makes_set_host(const scale_make_host_t *host);
// The makes' own files: the host handed over
extern const scale_make_host_t *scale_make_host;

// The makes
extern scale_handle_t and_fxi_scale_handle;
extern scale_handle_t steinberg_scale_handle;
extern scale_handle_t gng_scale_handle;
extern scale_handle_t ussolid_scale_handle;
extern scale_handle_t jm_science_scale_handle;
extern scale_handle_t creedmoor_scale_handle;
extern scale_handle_t radwag_ps_r2_scale_handle;
extern scale_handle_t sartorius_scale_handle;
extern scale_handle_t generic_scale_drv_handle;
extern scale_handle_t fa_en_scale_handle;

#ifdef __cplusplus
}
#endif

#endif  // SCALE_MAKE_H_
