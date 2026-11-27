#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <FreeRTOS.h>


typedef enum : uint8_t {
    DP_2 = 0,
    DP_3 = 1,
} decimal_places_t;


#ifdef __cplusplus
extern "C" {
#endif

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

/*
 * What a load found at a record's place (ADR-017, gate 1). A load never writes, and never leaves its module unusable:
 * the struct holds the defaults whatever is found, and the record is written only for a blank place (its defaults
 * stored), another version's record (stored at this version's size) and a save the power cut short (finished from the
 * spare slot). Anything else stays on the chip until the module is saved or the unit reset: its module runs on its
 * defaults meanwhile, and the screen says so (config_amiss)
 */
typedef enum {
    CONFIG_FOUND_NONE = 0,      // not loaded at this start
    CONFIG_FOUND_READ,          // read as stored, or stored since
    CONFIG_FOUND_MIGRATED,      // another version's record, stored at this version's size
    CONFIG_FOUND_FINISHED,      // a save the power cut short, finished from the spare slot
    CONFIG_FOUND_BLANK,         // never written (its first two bytes 0xFFFF): its defaults stored
    CONFIG_FOUND_DAMAGED,       // no record at any size, the same on two reads: the chip holds damage
    CONFIG_FOUND_UNSTEADY,      // no record at any size, read two ways: the bus at fault
    CONFIG_FOUND_UNREACHABLE,   // the EEPROM does not answer
    CONFIG_FOUND_NO_ROOM,       // no memory to read it
    CONFIG_FOUND_NOT_WRITTEN,   // read, but its write held on an update's trial failed when the update was kept
} config_found_t;

/**
 * @brief Load configuration from persistent storage: what was found at its place
 */
config_found_t load_config(uint16_t addr, void * cfg, const void * default_cfg, size_t size);

// What the load at addr found at this start, or became since (a save: CONFIG_FOUND_READ)
config_found_t config_found(uint16_t addr);
// The record's stored settings are what its module runs on: read, migrated or finished
bool config_settings_read(uint16_t addr);
// A finding the person should be told of: the module runs on settings that are not what the chip holds
bool config_found_amiss(config_found_t found);
// Each record amiss at this start, its address and what was found, up to room of them; their count
size_t config_amiss(uint16_t *addrs, config_found_t *found, size_t room);
// A finding in words, for the console
const char *config_found_text(config_found_t found);

/*
 * A record that is a header and an array of elements (the powders, the motors): each element may grow at its own end
 * (ADR-009, 2026-11-26). load_config_layout reads it as load_config does, and an earlier or a later version's record
 * element by element: each element's part kept, the rest at the defaults. load_config is a plain record's: a struct
 * that grows at its end only. Either way a field is only ever appended, never moved, removed or put to a new use
 */
/*
 * A field of a record's earlier layout that is not a prefix of today's (a field inserted before the rule: never insert,
 * always append): where it lay and its bytes then, where it lies and its bytes now
 */
typedef struct {
    uint16_t was;
    uint8_t was_size;
    uint16_t now;
    uint8_t now_size;
} config_field_map_t;

// A record's earlier layout, known by its stored size: each of its fields mapped into today's struct
typedef struct {
    uint16_t size;
    const config_field_map_t *fields;
    uint8_t field_count;
} config_past_layout_t;

typedef struct {
    size_t header;      // the bytes before the array (offsetof the array), never changed; 0 for a plain record
    size_t element;     // an element's bytes (sizeof); 0, a plain record with no array
    size_t count;       // the elements
    // A record whose history had an insertion (ADR-009's review, B1): the size it has been append-only from, below
    // which a record found is read only as one of its known earlier layouts, field by field; 0, append-only always
    size_t append_only_from;
    const config_past_layout_t *past;
    size_t past_count;
} config_layout_t;
config_found_t load_config_layout(uint16_t addr, void * cfg, const void * default_cfg, size_t size,
                                  const config_layout_t * layout);

/**
 * @brief Save configuration to persistent storage: whole or not at all (ADR-017, gate 1). The record goes first into
 * the spare slot, then into its place, each read back and compared; false when either did not take it, the record in
 * its place then as it was or, cut in its place, finished from the spare at the next start
 */
bool save_config(uint16_t addr, void * cfg, size_t size);

/*
 * A trial holds its writes (ADR-008, gate 2; the update's life cycle, gap 1). While an image runs on trial, save_config
 * writes nothing to the EEPROM: each record is held in RAM, the latest of each address, and written when the image
 * confirms itself. Gone back, nothing was written: the image it went back to reads its settings as it left them. A
 * record whose stored copy did not read (its layout changed) is held as its defaults, and said so at the confirmation
 */
void config_hold_writes(bool hold);
// Whether writes are held: an image runs on trial
bool config_holding_writes(void);
// Write what was held, and stop holding. The addresses of the records held as defaults, the settings the update
// reset, into reset (up to room of them); their count. A held write that fails is not dropped silently: its settings
// read as not saved, and it is amiss (CONFIG_FOUND_NOT_WRITTEN)
size_t config_write_held(uint16_t *reset, size_t room);
// A write that is never held: the update's own note, which a trial must leave as it goes (update.c)
bool save_config_now(uint16_t addr, void * cfg, size_t size);
// What the EEPROM holds of a record, its CRC checked (in its place, else a save cut short in the spare slot); nothing
// written. False when it does not read
bool config_read_stored(uint16_t addr, void * cfg, size_t size);
/*
 * What is stored of a part of a record (a powder of the eight, a motor of the two, or a whole record), as this version
 * reads it: on an update's trial the write held, else the record read with its CRC checked (the review's S1: a raw
 * read gave an earlier version's bytes at this version's offsets). False when no record read or stored at the start
 * holds it, or it does not read
 */
bool config_read_part(uint16_t addr, void * out, size_t size);

/**
 * @brief Whether the struct in RAM is what was last stored at that address, or read from it at the start: their
 * CRCs (ADR-003, gate 3: `saved`). So a change from the menu or the console counts as one from the web does.
 */
bool config_is_saved(uint16_t addr, const void * cfg, size_t size);

#ifdef __cplusplus
}
#endif


#endif // COMMON_H_