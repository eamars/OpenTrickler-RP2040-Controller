// The EEPROM on the host: 32 KB in RAM, erased to 0xFF as a new chip is (tests/host_test.py)
#include <string.h>

#include "eeprom.h"
#include "fake_eeprom.h"

static uint8_t fake_eeprom[FAKE_EEPROM_SIZE];


void fake_eeprom_erase(void) {
    memset(fake_eeprom, 0xFF, sizeof(fake_eeprom));
}


uint8_t *fake_eeprom_bytes(void) {
    return fake_eeprom;
}


bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len) {
    if ((size_t) data_addr + len > sizeof(fake_eeprom)) {
        return false;
    }
    memcpy(data, fake_eeprom + data_addr, len);
    return true;
}


bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len) {
    if ((size_t) data_addr + len > sizeof(fake_eeprom)) {
        return false;
    }
    memcpy(fake_eeprom + data_addr, data, len);
    return true;
}
