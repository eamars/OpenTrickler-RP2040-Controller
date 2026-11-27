#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define EEPROM_METADATA_BASE_ADDR               0 * 1024       // 0K: unused. It held the unit's id, now read from
                                                               // the Pico (unit_id.c, ADR-008, gate 4), and a unit
                                                               // may still hold that record: a module given the slot
                                                               // clears it first (its first two bytes are 0, as
                                                               // every record's must be: common.c)
#define EEPROM_SCALE_CONFIG_BASE_ADDR           1 * 1024       // 1K
#define EEPROM_WIRELESS_CONFIG_BASE_ADDR        2 * 1024       // 2K
#define EEPROM_MOTOR_CONFIG_BASE_ADDR           4 * 1024       // 4K
#define EEPROM_CHARGE_MODE_BASE_ADDR            5 * 1024       // 5K
#define EEPROM_APP_CONFIG_BASE_ADDR             6 * 1024       // 6k
#define EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR    7 * 1024       // 7k
#define EEPROM_MINI_12864_CONFIG_BASE_ADDR      8 * 1024       // 8k 
#define EEPROM_PROFILE_DATA_BASE_ADDR           9 * 1024       // 9k
#define EEPROM_SERVO_GATE_CONFIG_BASE_ADDR     10 * 1024       // 10k
#define EEPROM_UPDATE_CONFIG_BASE_ADDR         11 * 1024       // 11k: the update's channel and source (ADR-010)
#define EEPROM_UPDATE_NOTE_BASE_ADDR           12 * 1024       // 12k: the trial's note and how the last update ended (ADR-008)
                                                               // 13k to 20k and 22k: unused. ADR-009's first build
                                                               // (never released, the branch adr009_controller) kept
                                                               // each powder's calibration and the unit's record of
                                                               // its scale there, and a bench unit may still hold
                                                               // them: a module given these slots clears them first
#define EEPROM_SPARE_BASE_ADDR                 31 * 1024       // 31k: the spare slot, where a save writes its record
                                                               // first (ADR-017, gate 1; common.c). No firmware
                                                               // wrote here before

// Every record starts a slot of its own and is no longer than it: another version's record, shorter or longer, is
// looked for within its slot (common.c, load_config)
#define EEPROM_SLOT_SIZE                        1024

#ifdef __cplusplus
extern "C" {
#endif

// The lock and the bus, and whether the chip answers: false when it does not
bool eeprom_init(void);
// Whether the chip answered at the start (ADR-017, gate 1). When not, every module runs on its defaults, nothing is
// stored, and a charge and a calibration are refused (charge_mode.c, calibration.c)
bool eeprom_reachable(void);
bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len);
bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len);
// The EEPROM's lock, held across what must not be cut by another task's use of it: a whole save (the spare slot,
// then its place), the storage's tables (common.c). Taken again by the same task's reads and writes. Before the
// scheduler starts, nothing
void eeprom_lock(void);
void eeprom_unlock(void);

/*
 * A factory reset: every byte back to 0xFF. Refused while an image is on trial (the image it may go back to would find
 * nothing) and when the chip does not answer. Otherwise the EEPROM's lock is held from the erase on and never given
 * back, so no save lands between it and the restart, which the caller makes, saying first when the erase failed
 */
typedef enum {
    EEPROM_ERASED = 0,
    EEPROM_ERASE_FAILED,        // a page not written: the settings may be left in part
    EEPROM_ERASE_ON_TRIAL,      // refused: an image runs on trial
    EEPROM_ERASE_UNREACHABLE,   // refused: the chip does not answer
} eeprom_erase_t;
eeprom_erase_t eeprom_erase(void);


#ifdef __cplusplus
}
#endif

#endif  // EEPROM_H_