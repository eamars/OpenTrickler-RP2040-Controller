/*
 * Strips of WS2812 LEDs on the RP chips (chip_leds.h): a PIO state machine a strip, wherever one is free (ws2812.pio).
 */
#include "chip_leds.h"

#include <stddef.h>

#include "hardware/pio.h"
#include "ws2812.pio.h"

static PIO chip_leds_pio[CHIP_LEDS_STRIPS];
static uint chip_leds_sm[CHIP_LEDS_STRIPS];


bool chip_leds_open(unsigned int strip, unsigned int pin, bool rgbw) {
    PIO pio;
    uint sm;
    uint offset;
    if (strip >= CHIP_LEDS_STRIPS ||
        !pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, pin, 1, true)) {
        return false;
    }
    ws2812_program_init(pio, sm, offset, pin, 800000, rgbw);
    chip_leds_pio[strip] = pio;
    chip_leds_sm[strip] = sm;
    return true;
}


// The state machine shifts the word out from its top, 24 bits or 32: the LED's 24 are moved up to it
void chip_leds_put(unsigned int strip, uint32_t bits) {
    if (strip < CHIP_LEDS_STRIPS && chip_leds_pio[strip] != NULL) {
        pio_sm_put_blocking(chip_leds_pio[strip], chip_leds_sm[strip], bits << 8u);
    }
}
