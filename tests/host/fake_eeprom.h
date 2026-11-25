// The EEPROM on the host (fake_eeprom.c)
#ifndef FAKE_EEPROM_H_
#define FAKE_EEPROM_H_

#include <stdint.h>

#define FAKE_EEPROM_SIZE 32768

// Every byte back to 0xFF, as a new chip
void fake_eeprom_erase(void);
// The bytes themselves, for a test to read or damage
uint8_t *fake_eeprom_bytes(void);

#endif
