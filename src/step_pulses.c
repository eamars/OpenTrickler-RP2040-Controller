/*
 * The step pulses of the motors, on the RP chips' PIO: see step_pulses.h.
 */
#include <math.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "configuration.h"
#include "step_pulses.h"
#include "stepper.pio.h"

// A rate under this is a stop: the longest period the PIO's 32-bit count can hold is some 28 s
#define STEP_PULSES_MIN_RATE 0.05f


typedef struct {
    bool is_init;
    PIO pio;
    uint sm;
    uint offset;
    uint step_pin;

    // The pin's own counter, on whichever PIO had room
    bool has_counter;
    PIO counter_pio;
    uint counter_sm;

    volatile float rate_asked;      // Steps a second, set by anyone
    float rate_fed;                 // What the last segment was fed at, after rounding to the clock
    bool running;                   // A segment has been fed since the last stop

    uint32_t steps_fed;
    uint32_t underruns;
} step_pulses_channel_t;

static step_pulses_channel_t step_pulses_channels[STEP_PULSES_CHANNELS];
static int step_pulses_program_offset = -1;


bool step_pulses_init(unsigned int channel, unsigned int step_pin) {
    if (channel >= STEP_PULSES_CHANNELS) {
        return false;
    }
    step_pulses_channel_t *ch = &step_pulses_channels[channel];

    // The stepper program is loaded once and shared by the channels
    PIO pio = MOTOR_PIO;
    if (step_pulses_program_offset < 0) {
        if (!pio_can_add_program(pio, &stepper_program)) {
            printf("Step pulses: no room for the stepper program\n");
            return false;
        }
        step_pulses_program_offset = pio_add_program(pio, &stepper_program);
    }

    int sm = pio_claim_unused_sm(pio, false);
    if (sm < 0) {
        printf("Step pulses: no state machine free for channel %u\n", channel);
        return false;
    }

    ch->pio = pio;
    ch->sm = (uint) sm;
    ch->offset = (uint) step_pulses_program_offset;
    ch->step_pin = step_pin;
    stepper_program_init(ch->pio, ch->sm, ch->offset, step_pin);
    pio_sm_set_enabled(ch->pio, ch->sm, true);

    // The counter, on any PIO with a state machine to spare. Without one the steps fed stand in for the steps made
    PIO counter_pio;
    uint counter_sm;
    uint counter_offset;
    ch->has_counter = pio_claim_free_sm_and_add_program_for_gpio_range(&step_counter_program, &counter_pio, &counter_sm,
                                                                       &counter_offset, step_pin, 1, true);
    if (ch->has_counter) {
        ch->counter_pio = counter_pio;
        ch->counter_sm = counter_sm;
        step_counter_program_init(counter_pio, counter_sm, counter_offset, step_pin);
        pio_sm_set_enabled(counter_pio, counter_sm, true);
    }
    else {
        printf("Step pulses: no state machine free to count channel %u's steps\n", channel);
    }

    ch->is_init = true;
    return true;
}


void step_pulses_set_rate(unsigned int channel, float steps_per_s) {
    if (channel < STEP_PULSES_CHANNELS) {
        step_pulses_channels[channel].rate_asked = fabsf(steps_per_s);
    }
}


float step_pulses_get_rate(unsigned int channel) {
    return channel < STEP_PULSES_CHANNELS ? step_pulses_channels[channel].rate_fed : 0.0f;
}


// Stop now: what waits in the FIFO is dropped, and the state machine goes back to its first instruction, STEP low
static void step_pulses_stop(step_pulses_channel_t *ch) {
    pio_sm_set_enabled(ch->pio, ch->sm, false);
    pio_sm_clear_fifos(ch->pio, ch->sm);
    pio_sm_restart(ch->pio, ch->sm);
    pio_sm_exec(ch->pio, ch->sm, pio_encode_jmp(ch->offset));
    pio_sm_set_enabled(ch->pio, ch->sm, true);

    ch->running = false;
    ch->rate_fed = 0.0f;
}


static void step_pulses_feed_channel(step_pulses_channel_t *ch) {
    float rate = ch->rate_asked;

    if (!(rate >= STEP_PULSES_MIN_RATE)) {      // Also a rate that is not a number
        if (ch->running) {
            step_pulses_stop(ch);
        }
        return;
    }

    // One segment waits at most: a new rate is on the pin within two segments
    if (!pio_sm_is_tx_fifo_empty(ch->pio, ch->sm)) {
        return;
    }

    // Nothing waiting, and the state machine back at its first instruction: the pulses have paused for want of feed
    if (ch->running && pio_sm_get_pc(ch->pio, ch->sm) == ch->offset) {
        ch->underruns += 1;
    }

    // The period in clock cycles, at least the shortest the driver takes, in two halves
    float clock_hz = (float) clock_get_hz(clk_sys);
    float period_cycles = clock_hz / rate;
    uint32_t min_period = 2 * STEPPER_MIN_COUNT + STEPPER_HIGH_OVERHEAD_CYCLES + STEPPER_LOW_OVERHEAD_CYCLES;
    uint32_t period = period_cycles >= 4294967040.0f ? UINT32_MAX : (uint32_t) lroundf(period_cycles);
    if (period < min_period) {
        period = min_period;
    }
    uint32_t high_count = period / 2 - STEPPER_HIGH_OVERHEAD_CYCLES;
    uint32_t low_count = period - period / 2 - STEPPER_LOW_OVERHEAD_CYCLES;

    // As many steps as fill a segment, one at least
    uint32_t segment_cycles = (uint32_t) (clock_hz * STEP_PULSES_SEGMENT_MS / 1000.0f);
    uint32_t steps = segment_cycles / period;
    if (steps == 0) {
        steps = 1;
    }

    pio_sm_put(ch->pio, ch->sm, steps - 1);
    pio_sm_put(ch->pio, ch->sm, high_count);
    pio_sm_put(ch->pio, ch->sm, low_count);

    ch->steps_fed += steps;
    ch->rate_fed = clock_hz / (float) period;
    ch->running = true;
}


void step_pulses_feed(void) {
    for (unsigned int channel = 0; channel < STEP_PULSES_CHANNELS; channel += 1) {
        if (step_pulses_channels[channel].is_init) {
            step_pulses_feed_channel(&step_pulses_channels[channel]);
        }
    }
}


void step_pulses_get_stats(unsigned int channel, step_pulses_stats_t *stats) {
    step_pulses_channel_t *ch = &step_pulses_channels[channel];

    stats->steps_fed = ch->steps_fed;
    stats->underruns = ch->underruns;
    stats->counted_on_pin = ch->has_counter;
    stats->steps_made = ch->steps_fed;

    if (ch->has_counter) {
        // The counter is made to push its X: two instructions put in from here, between its own. It is waiting on
        // the pin, or about to; an instruction put in does not move its place in the program
        pio_sm_clear_fifos(ch->counter_pio, ch->counter_sm);
        pio_sm_exec(ch->counter_pio, ch->counter_sm, pio_encode_mov(pio_isr, pio_x));
        pio_sm_exec(ch->counter_pio, ch->counter_sm, pio_encode_push(false, false));
        stats->steps_made = 0u - pio_sm_get(ch->counter_pio, ch->counter_sm);
    }
}


void step_pulses_fault_stop(void) {
    // Registers only: this runs in a fault handler, with nothing of the firmware to rely on
    for (unsigned int channel = 0; channel < STEP_PULSES_CHANNELS; channel += 1) {
        step_pulses_channel_t *ch = &step_pulses_channels[channel];
        if (ch->is_init) {
            hw_clear_bits(&ch->pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + ch->sm));
            // The state machine may have stopped with STEP high: the pin is held low over it
            gpio_set_outover(ch->step_pin, GPIO_OVERRIDE_LOW);
        }
    }
}
