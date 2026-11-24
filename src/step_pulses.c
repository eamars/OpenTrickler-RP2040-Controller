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
static int step_counter_offsets[NUM_PIOS] = {[0 ... NUM_PIOS - 1] = -1};  // the counter's program, once on each PIO


// A state machine turned on or off by the CTRL register's set and clear aliases, one bit alone: the SDK's
// pio_sm_set_enabled() reads, changes and writes the whole register, and so undoes a change another core made between
// (ADR-008: the radio's state machine left off, its transfer waiting for ever)
static void step_pulses_sm_enable(PIO pio, uint sm, bool on) {
    if (on) {
        hw_set_bits(&pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + sm));
    }
    else {
        hw_clear_bits(&pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + sm));
    }
}


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
    step_pulses_sm_enable(ch->pio, ch->sm, true);

    // The counter, on a PIO other than the steppers', with a state machine to spare: the steppers' PIO is kept for the
    // two of them (a counter there took the second's). It is turned on once, here, so it may share the radio's PIO.
    // Without one the steps fed stand in for the steps made
    PIO counter_pio = NULL;
    uint counter_sm = 0;
    uint counter_offset = 0;
    ch->has_counter = false;
    for (uint idx = 0; idx < NUM_PIOS && !ch->has_counter; idx += 1) {
        PIO candidate = pio_get_instance(idx);
        if (candidate == MOTOR_PIO) {
            continue;
        }
        int offset = step_counter_offsets[idx];
        if (offset < 0 && pio_can_add_program(candidate, &step_counter_program)) {
            offset = pio_add_program(candidate, &step_counter_program);
            step_counter_offsets[idx] = offset;
        }
        int counter = offset >= 0 ? pio_claim_unused_sm(candidate, false) : -1;
        if (counter >= 0) {
            counter_pio = candidate;
            counter_sm = (uint) counter;
            counter_offset = (uint) offset;
            ch->has_counter = true;
        }
    }
    if (ch->has_counter) {
        ch->counter_pio = counter_pio;
        ch->counter_sm = counter_sm;
        step_counter_program_init(counter_pio, counter_sm, counter_offset, step_pin);
        step_pulses_sm_enable(counter_pio, counter_sm, true);
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
    step_pulses_sm_enable(ch->pio, ch->sm, false);
    pio_sm_clear_fifos(ch->pio, ch->sm);
    pio_sm_restart(ch->pio, ch->sm);
    pio_sm_exec(ch->pio, ch->sm, pio_encode_jmp(ch->offset));
    step_pulses_sm_enable(ch->pio, ch->sm, true);

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

    // As many steps as last a segment at least: this is fed every millisecond and one segment waits, so a segment
    // shorter than two feeds leaves the pulses waiting on a late one. Rounded down, a segment at just under 1,000
    // steps a second lasted 1.0 ms and the fine motor paused 16 times in 10 s (ADR-009, 2026-11-22)
    uint32_t segment_cycles = (uint32_t) (clock_hz * STEP_PULSES_SEGMENT_MS / 1000.0f);
    uint32_t steps = (segment_cycles + period - 1) / period;

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
