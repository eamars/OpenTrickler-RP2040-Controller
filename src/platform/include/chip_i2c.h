#ifndef CHIP_I2C_H_
#define CHIP_I2C_H_

/*
 * An I2C bus, the chip its master (ADR-006, gate 7), by its number in the board's pin map (configuration.h). A
 * platform interface; the RP chips' is the Pico SDK's (platform/rp2/chip_i2c_rp2.c). No lock here: whoever uses a bus
 * holds one across its calls (the storage, eeprom.c). A write and the read after it are one call, with no stop between
 * them, as ESP-IDF's driver has them.
 *
 * Every call returns within a time limit (ADR-017, gate 1): a byte that takes longer than CHIP_I2C_BYTE_LIMIT_US is a
 * failure, so a bus held low, or a device that stretches the clock without end, fails the call and never hangs it.
 * ESP-IDF's calls take a whole transfer's limit (`i2c_master_transmit`'s `xfer_timeout_ms`): the bytes times this.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The longest a byte may take, at any rate: at 400 kHz one takes 23 us, at 100 kHz 90 us
#define CHIP_I2C_BYTE_LIMIT_US 1000u

// The bus on its pins at its rate, with the chip's own pull-ups on both pins when `pull_ups`
void chip_i2c_open(unsigned int bus, unsigned int sda_pin, unsigned int scl_pin, uint32_t rate_hz, bool pull_ups);

// Bytes to the device at `addr`, then a stop. False unless the device took every byte
bool chip_i2c_write(unsigned int bus, uint8_t addr, const uint8_t *data, size_t len);

// Bytes to the device, then `in_len` read from it with no stop between (a random read). False unless both were whole
bool chip_i2c_write_read(unsigned int bus, uint8_t addr, const uint8_t *out, size_t out_len, uint8_t *in,
                         size_t in_len);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_I2C_H_
