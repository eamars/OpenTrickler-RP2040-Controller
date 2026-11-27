#ifndef CHIP_LEDS_H_
#define CHIP_LEDS_H_

/*
 * Strips of WS2812 LEDs, each on a pin, by a number its user gives it (ADR-006, gate 7). A platform interface; the RP
 * chips' is a PIO state machine a strip (ws2812.pio, platform/rp2/chip_leds_rp2.c). An LED takes the bits it is sent
 * at 800 kHz, the first sent highest: a WS2812's green, red, blue.
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHIP_LEDS_STRIPS 2

// A strip on its pin, its LEDs 24 bits each, or 32 with a white (`rgbw`). False when the chip has no room for it
bool chip_leds_open(unsigned int strip, unsigned int pin, bool rgbw);

/*
 * The next LED's bits: the low 24 of `bits`, the first sent highest; an RGBW LED's last 8, its white, are sent as 0,
 * as they always were. It waits while the strip's queue is full. Nothing, on a strip not open
 */
void chip_leds_put(unsigned int strip, uint32_t bits);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_LEDS_H_
