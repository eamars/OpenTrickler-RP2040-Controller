#ifndef CHIP_SERVO_H_
#define CHIP_SERVO_H_

/*
 * Two servos' pulses at 50 Hz, on two pins of the chip (ADR-006, gate 7): the servo gate's two shutters. A platform
 * interface; the RP chips' is one PWM slice for both pins, its two channels (platform/rp2/chip_servo_rp2.c), so both
 * widths change at the same moment.
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The two pins' pulses started at 50 Hz, each 0 wide until set. False when the two cannot share one timer
bool chip_servo_open(unsigned int pin0, unsigned int pin1);

/*
 * Both pulses' widths, set at the same moment, each a share of the 20 ms period with 65535 the whole (the RP chips'
 * period is 65,466 counts at 125 MHz, so a width is that much the longer)
 */
void chip_servo_set(uint16_t width0, uint16_t width1);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_SERVO_H_
