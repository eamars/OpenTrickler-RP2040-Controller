/*
 * An SPI bus on the RP chips (chip_spi.h): spi0 and spi1. spi_init() gives 8 bits, mode 0, the highest bit first.
 */
#include "chip_spi.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"


static spi_inst_t *chip_spi_instance(unsigned int bus) {
    return bus == 0 ? spi0 : spi1;
}


void chip_spi_open(unsigned int bus, unsigned int sck_pin, unsigned int tx_pin, uint32_t rate_hz) {
    spi_init(chip_spi_instance(bus), rate_hz);
    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(tx_pin, GPIO_FUNC_SPI);
}


// spi_write_blocking() returns once the bus is idle: the last bit is out
void chip_spi_write(unsigned int bus, const uint8_t *data, size_t len) {
    spi_write_blocking(chip_spi_instance(bus), data, len);
}
