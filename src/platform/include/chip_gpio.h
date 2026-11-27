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

// A pin made an input, with the chip's pull-up on it when `pull_up`
void chip_gpio_input(unsigned int pin, bool pull_up);

// A pin's level. From a task or an interrupt
bool chip_gpio_get(unsigned int pin);

// The edges an input's interrupt comes on
#define CHIP_GPIO_EDGE_FALL 0x1u
#define CHIP_GPIO_EDGE_RISE 0x2u
#define CHIP_GPIO_EDGE_BOTH (CHIP_GPIO_EDGE_FALL | CHIP_GPIO_EDGE_RISE)

/*
 * What an input's edge calls. It runs in an interrupt: short, and of FreeRTOS only its ...FromISR calls. It is told
 * the pin, not the edge: it reads the level itself, as ESP-IDF's handlers must
 */
typedef void (*chip_gpio_edge_fn)(unsigned int pin);

// `on_edge` called at the pin's `edges` from now on, one a pin. False for a pin the chip cannot watch
bool chip_gpio_on_edges(unsigned int pin, unsigned int edges, chip_gpio_edge_fn on_edge);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_GPIO_H_
