#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define EEPROM_METADATA_BASE_ADDR               0 * 1024       // 0K: unused. It held the unit's id, now read from
                                                               // the Pico (unit_id.c, ADR-008, gate 4), and a unit
                                                               // may still hold that record: a module given the slot
                                                               // clears it first (its first two bytes, 0, pass
                                                               // load_config's old revision check)
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

// Every record starts a slot of its own and is no longer than it: another version's record, shorter or longer, is
// looked for within its slot (common.c, load_config)
#define EEPROM_SLOT_SIZE                        1024

// EEPROM save handler function
typedef bool (*eeprom_save_handler_t)(void);


#ifdef __cplusplus
extern "C" {
#endif

bool eeprom_init(void);
bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len);
bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len);

/*
 * Fill all EEPROM with 0xFF
 */
uint8_t eeprom_erase(bool);
uint8_t eeprom_save_all(void);
void eeprom_register_handler(eeprom_save_handler_t handler);


#ifdef __cplusplus
}
#endif

#endif  // EEPROM_H_