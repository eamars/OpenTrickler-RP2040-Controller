// The EEPROM on the host (fake_eeprom.c)
#ifndef FAKE_EEPROM_H_
#define FAKE_EEPROM_H_

#include <stdbool.h>
#include <stdint.h>

#define FAKE_EEPROM_SIZE 32768

// Every byte back to 0xFF, as a new chip, and no fault put in
void fake_eeprom_erase(void);
// The bytes themselves, for a test to read or damage
uint8_t *fake_eeprom_bytes(void);
// The chip answers, or not at all
void fake_eeprom_set_reachable(bool reachable);
// The power goes once the writes have put this many bytes more: the rest of that write, and every one after, lost
void fake_eeprom_cut_after(long bytes);
// The byte at addr read one way, then the other, as a bus at fault gives it
void fake_eeprom_unsteady(long addr);
// The byte at addr left as it was by every write, with no word: a write that does not take
void fake_eeprom_drop(long addr);

#endif
