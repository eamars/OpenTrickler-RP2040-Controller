#ifndef CHIP_SPI_H_
#define CHIP_SPI_H_

/*
 * An SPI bus, written only, the chip its master (ADR-006, gate 7), by its number in the board's pin map
 * (configuration.h): the display's. A platform interface; the RP chips' is the Pico SDK's (platform/rp2/chip_spi_rp2.c).
 * The device's chip select and its other pins are plain outputs (chip_gpio.h), set by its driver between writes.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The bus on its clock and data pins at `rate_hz`: 8 bits a word, mode 0, the highest bit first
void chip_spi_open(unsigned int bus, unsigned int sck_pin, unsigned int tx_pin, uint32_t rate_hz);

/*
 * Bytes sent, any number. It returns once the last bit is out, so a pin set after it changes between bytes, never
 * during one (the display's A0)
 */
void chip_spi_write(unsigned int bus, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_SPI_H_
