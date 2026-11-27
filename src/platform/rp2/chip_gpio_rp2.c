/*
 * The pins on the RP chips (chip_gpio.h).
 */
#include "chip_gpio.h"

#include "hardware/gpio.h"


void chip_gpio_output(unsigned int pin, bool level) {
    gpio_init(pin);
    gpio_put(pin, level);   // the level first, so the pin never shows another
    gpio_set_dir(pin, GPIO_OUT);
}


void chip_gpio_put(unsigned int pin, bool level) {
    gpio_put(pin, level);
}
