#ifndef CHARGE_PROFILES_H_
#define CHARGE_PROFILES_H_
/*
 * The charge profiles (ADR-014), "Loads" to a person: what is thrown, kept apart from how a powder flows. Each is a
 * name, a target and a powder profile's id; there are eight, and one is always chosen. The charge's target and powder
 * are the chosen one's own fields: the unit has no second, working target.
 */
#include <stdbool.h>
#include <stdint.h>

#include "activity.h"
#include "common.h"

#define CHARGE_PROFILE_NAME_LEN     16
#define CHARGE_PROFILE_COUNT        8

/*
    A charge profile, eight to the record (eeprom_charge_profile_data_t, a header and an array: load_config_layout). A
    field is only ever appended at its end, so each version reads the profiles of an earlier or a later one
*/
typedef struct {
    char name[CHARGE_PROFILE_NAME_LEN];
    float target;               // in the scale's unit; 0: not set
    uint8_t powder_profile;     // a powder profile's id
    uint8_t unused[3];          // named, so nothing is appended into padding
} charge_profile_t;

typedef struct {
    uint16_t zero;              // a record's first two bytes
    uint16_t chosen;
    charge_profile_t profiles[CHARGE_PROFILE_COUNT];
} eeprom_charge_profile_data_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The record (charge_profiles.c), which needs the storage alone. The load, after the powders': the defaults are
 * "Load 1" to "Load 8", no target, each on powder_stored (the powders' stored choice), the first chosen. What a
 * record holds that no unit can run on runs as the first in RAM, not stored: a chosen index past the eight, a powder's
 * id past powder_count; a target that is no weight is not set, a name with no ending is ended
 */
config_found_t charge_profiles_load(uint16_t powder_stored, uint8_t powder_count);
bool charge_profiles_save(void);
// The eight and the choice in RAM as stored
bool charge_profiles_is_saved(void);

uint8_t charge_profiles_chosen(void);
// A profile to read; NULL for no such id. Its name always ends
const charge_profile_t *charge_profiles_get(uint8_t id);
// The chosen one's target; 0, not set
float charge_profiles_target(void);

/*
 * The module's start (charge_profiles_settings.c): the load, the powder in use set to the chosen one's, and the
 * settings' table (/api/v1/charge_profiles/{id}). After the powders' start
 */
bool charge_profiles_init(void);
// What a target must pass beside the table's limits (what the screen's digits can show), handed over by the charge
// at its start; with none handed over, the limits alone
void charge_profiles_set_target_check(bool (*fits)(float target));

/*
 * The writes the charge, the screen and the API share, each live and not stored: NULL when done, else why not. Another
 * profile chosen brings its target and its powder. The chosen one's target and powder are written as a PATCH of its
 * resource writes them: the same checks, refusals and event. All three are refused while a throw is under way
 */
const char *charge_profiles_choose(uint8_t id);
const char *charge_profiles_set_target(float target, activity_by_t by);
const char *charge_profiles_set_powder(uint8_t powder_profile, activity_by_t by);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_PROFILES_H_
