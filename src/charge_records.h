#ifndef CHARGE_RECORDS_H_
#define CHARGE_RECORDS_H_

/*
    What this unit measured, stored (ADR-009, gate R; the parameter register's row 9). Two kinds of record:
    - the unit's record (EEPROM 22 K): the scale's digit and interval, and its lag and rest, unset until gate 3
      measures them;
    - a powder's record for each powder profile (13 to 20 K, a slot each): each tube's grains a turn, its stops and
      the small moves as measured, and the one list of carried stops with the law that learnt it.
    Each carries the id of the unit it was measured on, and the unit refuses a write that names another (409
    not_this_unit): another unit's own data is never applied here. Their stored form is their own, apart from the
    controller's structs; the band and the finish are not in it (made at a charge's start).

    Read into a working form at a charge's start and by the console, never during a throw; written between charges
    only. The API reads and writes them as settings (settings/unit_record, settings/powder_records/{id}), each written
    through at once; the export holds them, so a backup is whole.
*/

#include <stdbool.h>
#include <stdint.h>

#include "charge_calibration.h"

#define CHARGE_RECORD_VERSION 2
// A powder's record: version 3 since ADR-009's gate F.3 (the back-off's spill appended); the unit's record stays 2
#define CHARGE_POWDER_RECORD_VERSION 3
#define CHARGE_RECORD_COARSE_STOPS 4
#define CHARGE_RECORD_FINE_STOPS 8
#define CHARGE_RECORD_MOVES 3
#define CHARGE_RECORD_ERRORS CHARGE_CALIBRATION_FIRST_ERRORS_MAX
#define CHARGE_RECORD_UNIT_ID_ROOM 12

typedef enum {
    CHARGE_RECORD_OK = 0,
    CHARGE_RECORD_NONE,         // nothing stored: a powder never calibrated
    CHARGE_RECORD_DAMAGED,      // something stored that fails its CRC or its version: not used, and said
} charge_record_state_t;

#ifdef __cplusplus
extern "C" {
#endif

// At the start, the EEPROM read: the unit's record loaded, and each powder's slot of the form before gate R (v1)
// made a v2 record once, the PID law's old lists (21 K) zeroed. Says on the console what it did
void charge_records_init(void);
// Their routes, as settings resources
bool charge_records_register(void);

// A powder's record into the charge's working form (its digit and interval the unit record's), and the law that
// learnt its carried stops (0 the glide, 1 the PID law)
charge_record_state_t charge_records_load(uint8_t profile, charge_calibration_record_t *out, uint8_t *law);
// The working form stored as the powder's record, its carried stops the law's; the digit and interval into the
// unit's record. False when the EEPROM did not take it
bool charge_records_save(uint8_t profile, const charge_calibration_record_t *record, uint8_t law);
// The console's `ccal corrupt <profile>`: a byte of the stored record flipped, for the proof that a damaged record is
// not used. Never in the API
bool charge_records_corrupt(uint8_t profile);
// In words, for a charge refused for it
const char *charge_records_state_words(charge_record_state_t state);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_RECORDS_H_
