// The EEPROM on the host: 32 KB in RAM, erased to 0xFF as a new chip is (tests/host_test.py), and the faults a real
// one has, each put in by a test (ADR-017, gate 1)
#include <string.h>

#include "eeprom.h"
#include "fake_eeprom.h"

static uint8_t fake_eeprom[FAKE_EEPROM_SIZE];
static bool fake_reachable = true;
static long fake_write_budget = -1;     // bytes the writes may still put before the power goes; -1: no cut
static long fake_unsteady_at = -1;      // a byte read one way, then the other; -1: none
static unsigned fake_unsteady_reads = 0;
static long fake_dropped_at = -1;       // a byte a write leaves as it was, and says nothing; -1: none


void fake_eeprom_erase(void) {
    memset(fake_eeprom, 0xFF, sizeof(fake_eeprom));
    fake_reachable = true;
    fake_write_budget = -1;
    fake_unsteady_at = -1;
    fake_dropped_at = -1;
}


uint8_t *fake_eeprom_bytes(void) {
    return fake_eeprom;
}


void fake_eeprom_set_reachable(bool reachable) {
    fake_reachable = reachable;
}


void fake_eeprom_cut_after(long bytes) {
    fake_write_budget = bytes;
}


void fake_eeprom_unsteady(long addr) {
    fake_unsteady_at = addr;
}


void fake_eeprom_drop(long addr) {
    fake_dropped_at = addr;
}


bool eeprom_reachable(void) {
    return fake_reachable;
}


bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len) {
    if (!fake_reachable || (size_t) data_addr + len > sizeof(fake_eeprom)) {
        return false;
    }
    memcpy(data, fake_eeprom + data_addr, len);
    if (fake_unsteady_at >= data_addr && fake_unsteady_at < (long) (data_addr + len)) {
        fake_unsteady_reads += 1;
        data[fake_unsteady_at - data_addr] ^= (uint8_t) (fake_unsteady_reads & 1u);
    }
    return true;
}


bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len) {
    if (!fake_reachable || (size_t) data_addr + len > sizeof(fake_eeprom)) {
        return false;
    }
    size_t put = len;
    if (fake_write_budget >= 0 && (long) put > fake_write_budget) {
        put = (size_t) fake_write_budget;
    }
    for (size_t idx = 0; idx < put; idx += 1) {
        if ((long) (data_addr + idx) != fake_dropped_at) {
            fake_eeprom[data_addr + idx] = data[idx];
        }
    }
    if (fake_write_budget >= 0) {
        fake_write_budget -= (long) put;
    }
    return put == len;
}
