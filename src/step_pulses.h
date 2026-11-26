#ifndef STEP_PULSES_H_
#define STEP_PULSES_H_

/*
 * The step pulses of the motors, on the RP chips' PIO (ADR-012, step 2). "This many steps a second", "stop",
 * "steps made so far": it knows the PIO and the STEP pins, and nothing of drivers, ramps or tricklers.
 *
 * The PIO makes only the steps it is fed (stepper.pio). step_pulses_feed() tops each running channel up, a few
 * milliseconds at a time, and must be called every millisecond: when the calls stop, the pulses stop within two
 * segments, each STEP_PULSES_SEGMENT_MS and less than a step more (or one step, when a step is longer). A rate of
 * zero stops a channel at its next feed.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STEP_PULSES_CHANNELS 2

// The pulses fed at a time. Two segments at most are ahead of the pin: the one being made and the one waiting
#define STEP_PULSES_SEGMENT_MS 2

typedef struct {
    uint32_t steps_made;        // Counted on the STEP pin
    uint32_t steps_fed;         // Handed to the PIO; those of a segment cut short by a stop were not all made
    uint32_t underruns;         // Feeds that found the PIO waiting with nothing to do while the channel should run
    bool counted_on_pin;        // False if no state machine was free to count: steps_made is then steps_fed
} step_pulses_stats_t;

// Claim the PIO for one channel's STEP pin. False, and said on the console, if there is no room
bool step_pulses_init(unsigned int channel, unsigned int step_pin);

// The rate from the next feed on, in steps a second; its sign is ignored (the direction pin is the caller's)
void step_pulses_set_rate(unsigned int channel, float steps_per_s);

// The rate the PIO was last fed, in steps a second, after rounding to its clock
float step_pulses_get_rate(unsigned int channel);

// Top up every channel. Every millisecond, from one task
void step_pulses_feed(void);

void step_pulses_get_stats(unsigned int channel, step_pulses_stats_t *stats);

// For a fault handler: every channel's pulses stopped by register writes alone. Not undone short of a restart
void step_pulses_fault_stop(void);

#ifdef __cplusplus
}
#endif

#endif  // STEP_PULSES_H_
