#ifndef CAT24C256_EEPROM_H_
#define CAT24C256_EEPROM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
    The CAT24C256, the controller board's EEPROM: 32 KB on I2C, written a page of 64 bytes at a time. No lock here:
    the storage above it holds one across a read or a write (eeprom.c).
*/

#ifdef __cplusplus
extern "C" {
#endif

// The I2C port and its pins
void cat24c256_eeprom_init();
// Whether the chip answers: a byte read from its first address (ADR-017, gate 1)
bool cat24c256_present();
// Bytes from an address, across pages: a page at a time from an address that starts one. False unless every byte was
// taken (a write) or given (a read) within the bus's time limit (chip_i2c.h)
bool cat24c256_write(uint16_t data_addr, uint8_t * data, size_t len);
bool cat24c256_read(uint16_t data_addr, uint8_t * data, size_t len);
// Every byte back to 0xFF, as a new chip
bool cat24c256_eeprom_erase();

#ifdef __cplusplus
}
#endif

#endif  // CAT24C256_EEPROM_H_
