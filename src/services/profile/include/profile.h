#ifndef PROFILE_H_
#define PROFILE_H_

#include <stdint.h>
#include <stdbool.h>


#define PROFILE_NAME_MAX_LEN    16
#define MAX_PROFILE_CNT         8


/*
    A powder, eight to the record (eeprom_profile_data_t, a header and an array: load_config_layout). A field is only
    ever appended at its end, so each version reads the powders of an earlier or a later one (ADR-009, 2026-11-26)
*/
typedef struct
{
    uint32_t rev;
    uint32_t compatibility;     // as v1.2x stored it (its REST API's p1): kept, used by nothing, not in the API

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


typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;

    profile_t profiles[MAX_PROFILE_CNT];
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