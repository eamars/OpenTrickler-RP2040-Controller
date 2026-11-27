/*
 * The pins on the RP chips (chip_gpio.h). The SDK takes one callback for every pin's interrupt; it is ours, and calls
 * each pin's own from a table.
 */
#include "chip_gpio.h"

#include <stddef.h>

#include "hardware/gpio.h"

static chip_gpio_edge_fn chip_gpio_edge_calls[NUM_BANK0_GPIOS];


void chip_gpio_output(unsigned int pin, bool level) {
    gpio_init(pin);
    gpio_put(pin, level);   // the level first, so the pin never shows another
    gpio_set_dir(pin, GPIO_OUT);
}


void chip_gpio_put(unsigned int pin, bool level) {
    gpio_put(pin, level);
}


void chip_gpio_input(unsigned int pin, bool pull_up) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (pull_up) {
        gpio_pull_up(pin);
    }
}


bool chip_gpio_get(unsigned int pin) {
    return gpio_get(pin);
}


static void chip_gpio_irq(uint gpio, uint32_t events) {
    (void) events;
    if (gpio < NUM_BANK0_GPIOS && chip_gpio_edge_calls[gpio] != NULL) {
        chip_gpio_edge_calls[gpio](gpio);
    }
}


bool chip_gpio_on_edges(unsigned int pin, unsigned int edges, chip_gpio_edge_fn on_edge) {
    if (pin >= NUM_BANK0_GPIOS) {
        return false;
    }
    uint32_t events = ((edges & CHIP_GPIO_EDGE_FALL) != 0 ? GPIO_IRQ_EDGE_FALL : 0) |
                      ((edges & CHIP_GPIO_EDGE_RISE) != 0 ? GPIO_IRQ_EDGE_RISE : 0);
    chip_gpio_edge_calls[pin] = on_edge;
    // The callback is this core's for every pin: set again with each, the same one
    gpio_set_irq_enabled_with_callback(pin, events, true, chip_gpio_irq);
    return true;
}
