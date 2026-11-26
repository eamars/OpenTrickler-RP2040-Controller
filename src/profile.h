#ifndef PROFILE_H_
#define PROFILE_H_

#include <stdint.h>
#include <stdbool.h>


#define PROFILE_NAME_MAX_LEN    16
#define MAX_PROFILE_CNT         8

#define EEPROM_PROFILE_DATA_REV             1           // 16 bit
#define PROFILE_RECORD_REV                  1           // never stored: a powder's record validates by its CRC

/*
    A powder: a record of its own in the EEPROM, a slot each from EEPROM_POWDER_BASE_ADDR (ADR-009, 2026-11-26), so a
    field is appended at the record's end and an earlier or a later version reads the fields it knows (common.c).
    Never a field put to a new use: a new one is appended
*/
typedef struct
{
    uint32_t rev;               // 0: the loader reads the first two bytes as an old revision (common.c)

    char name[PROFILE_NAME_MAX_LEN];

    float coarse_kp;
    float coarse_ki;
    float coarse_kd;

    float coarse_min_flow_speed_rps;
    float coarse_max_flow_speed_rps;

    float fine_kp;
    float fine_ki;
    float fine_kd;

    float fine_min_flow_speed_rps;
    float fine_max_flow_speed_rps;

    // Appended by ADR-009: each tube's weight a turn where it streams, measured by the powder's calibration; 0, not
    // measured (null in the API: never calibrated). The fine tube's is step 2's seed
    float fine_weight_per_turn;
    float coarse_weight_per_turn;
} profile_t;


// The powder chosen, a record of its own at EEPROM_PROFILE_DATA_BASE_ADDR
typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;
} eeprom_profile_data_t;


#ifdef __cplusplus
extern "C" {
#endif



// Interface
bool profile_data_init(void);
bool profile_data_save();

profile_t * profile_select(uint8_t idx);
profile_t * profile_get_selected();
// A profile by its index, the selection left as it is; NULL for no such profile
profile_t * profile_get(uint16_t idx);
uint16_t profile_get_selected_idx();
// A profile's name, ended even when it was stored with none; NULL for no such profile. Good until the next call
const char *profile_get_name(uint16_t idx);
// The profiles in RAM as stored, the one chosen with them (ADR-003, gate 3: `saved`)
bool profile_data_is_saved(void);

// REST interface


#ifdef __cplusplus
}
#endif

#endif  // PROFILE_H_