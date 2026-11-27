#ifndef CHIP_GPIO_H_
#define CHIP_GPIO_H_

/*
 * The chip's pins as plain inputs and outputs (ADR-006, gate 7), by their number in the board's pin map
 * (configuration.h). A platform interface; the RP chips' is the Pico SDK's (platform/rp2/chip_gpio_rp2.c). A pin a bus
 * takes (a serial port's, the SPI's, the I2C's) is given to that bus's open, not set here.
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A pin made an output, driving `level` from its first moment
void chip_gpio_output(unsigned int pin, bool level);

// An output's level. From a task or an interrupt
void chip_gpio_put(unsigned int pin, bool level);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_GPIO_H_
