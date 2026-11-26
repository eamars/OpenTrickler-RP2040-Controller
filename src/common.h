#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include "hardware/pio.h"


typedef enum {
    DP_2 = 0,
    DP_3 = 1,
} decimal_places_t;


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PIO pio;
    int sm;
} pio_config_t;


/**
 * If the RTOS is running then use RTOS delay. Otherwise use dummy delay. 
*/
void delay_ms(uint32_t ms, BaseType_t scheduler_state);

/*
    A weight as text, with the buffer's length: never written past it, and an empty string for a number of decimals
    that is neither (ADR-012; audit M5). float_to_string writes with no length and leaves the buffer as it was.
*/
int weight_to_string(char * output_decimal_str, size_t output_size, float var, decimal_places_t decimal_places);

const char * boolean_to_string(bool var);
bool string_to_boolean(char * s);

int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places);

/**
 * @brief CRC-32 (zlib's) of bytes that come in pieces: the CRC so far, 0 at the start. The same value as Python's
 * zlib.crc32 of the whole
 */
uint32_t software_crc32_continue(uint32_t crc, const void * data, size_t length);

/** 
 * @brief Load configuration from persistent storage. 
 */
bool load_config(uint16_t addr, void * cfg, const void * default_cfg, size_t size, uint16_t rev_validation);

/*
 * A record that is a header and an array of elements (the powders, the motors): each element may grow at its own end
 * (ADR-009, 2026-11-26). load_config_layout reads it as load_config does, and an earlier or a later version's record
 * element by element: each element's part kept, the rest at the defaults. load_config is a plain record's: a struct
 * that grows at its end only. Either way a field is only ever appended, never moved, removed or put to a new use
 */
typedef struct {
    size_t header;      // the bytes before the array (offsetof the array), never changed
    size_t element;     // an element's bytes (sizeof)
    size_t count;       // the elements
} config_layout_t;
bool load_config_layout(uint16_t addr, void * cfg, const void * default_cfg, size_t size,
                        const config_layout_t * layout, uint16_t rev_validation);

/**
 * @brief Save configuration to persistent storage
 */
bool save_config(uint16_t addr, void * cfg, size_t size);

/*
 * A trial holds its writes (ADR-008, gate 2; the update's life cycle, gap 1). While an image runs on trial, save_config
 * writes nothing to the EEPROM: each record is held in RAM, the latest of each address, and written when the image
 * confirms itself. Gone back, nothing was written: the image it went back to reads its settings as it left them. A
 * record whose stored copy did not read (its layout changed) is held as its defaults, and said so at the confirmation
 */
void config_hold_writes(bool hold);
// Write what was held, and stop holding. The addresses of the records held as defaults, the settings the update
// reset, into reset (up to room of them); their count
size_t config_write_held(uint16_t *reset, size_t room);
// Whether the record at addr is held as its defaults: its stored copy did not read on this trial
bool config_defaults_held(uint16_t addr);
// A write that is never held: the update's own note, which a trial must leave as it goes (update.c)
bool save_config_now(uint16_t addr, void * cfg, size_t size);
// What the EEPROM holds of a record, its CRC checked; nothing written. False when it does not read
bool config_read_stored(uint16_t addr, void * cfg, size_t size);

/**
 * @brief Whether the struct in RAM is what was last stored at that address, or read from it at the start: their
 * CRCs (ADR-003, gate 3: `saved`). So a change from the menu or the console counts as one from the web does.
 */
bool config_is_saved(uint16_t addr, const void * cfg, size_t size);

#ifdef __cplusplus
}
#endif


#endif // COMMON_H_