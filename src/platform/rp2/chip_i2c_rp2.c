/*
 * An I2C bus on the RP chips (chip_i2c.h): i2c0 and i2c1.
 */
#include "chip_i2c.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"


static i2c_inst_t *chip_i2c_instance(unsigned int bus) {
    return bus == 0 ? i2c0 : i2c1;
}


void chip_i2c_open(unsigned int bus, unsigned int sda_pin, unsigned int scl_pin, uint32_t rate_hz, bool pull_ups) {
    i2c_init(chip_i2c_instance(bus), rate_hz);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    if (pull_ups) {
        gpio_pull_up(sda_pin);
        gpio_pull_up(scl_pin);
    }
}


bool chip_i2c_write(unsigned int bus, uint8_t addr, const uint8_t *data, size_t len) {
    return i2c_write_blocking(chip_i2c_instance(bus), addr, data, len, false) != PICO_ERROR_GENERIC;
}


bool chip_i2c_write_read(unsigned int bus, uint8_t addr, const uint8_t *out, size_t out_len, uint8_t *in,
                         size_t in_len) {
    i2c_inst_t *i2c = chip_i2c_instance(bus);
    bool written = i2c_write_blocking(i2c, addr, out, out_len, true) == (int) out_len;
    return written && i2c_read_blocking(i2c, addr, in, in_len, false) == (int) in_len;
}
