/*
 * The charge profiles' record (ADR-014): its load, its defaults, what is mended in RAM, and its reads. It needs the
 * storage alone, so the PC build runs it over an EEPROM in RAM (tests/host)
 */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "charge_profiles.h"
#include "charge_profiles_record.h"
#include "common.h"
#include "eeprom.h"


static eeprom_charge_profile_data_t charge_profile_data;


eeprom_charge_profile_data_t *charge_profiles_record(void) {
    return &charge_profile_data;
}


config_found_t charge_profiles_load(uint16_t powder_stored, uint8_t powder_count) {
    eeprom_charge_profile_data_t defaults;
    memset(&defaults, 0, sizeof(defaults));
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT; idx += 1) {
        snprintf(defaults.profiles[idx].name, CHARGE_PROFILE_NAME_LEN, "Load %d", idx + 1);
        defaults.profiles[idx].powder_profile = (uint8_t) (powder_stored < powder_count ? powder_stored : 0);
    }

    // A header and eight profiles: each profile of an earlier or a later version read for the fields it has
    static const config_layout_t layout = {.header = offsetof(eeprom_charge_profile_data_t, profiles),
                                           .element = sizeof(charge_profile_t), .count = CHARGE_PROFILE_COUNT};
    // Whatever is found, the struct holds profiles to run on (ADR-017, gate 1)
    config_found_t found = load_config_layout(EEPROM_CHARGE_PROFILES_BASE_ADDR, &charge_profile_data, &defaults,
                                              sizeof(eeprom_charge_profile_data_t), &layout);

    // Mended in RAM only: the record then reads as not stored, and the chip keeps what it held
    if (charge_profile_data.chosen >= CHARGE_PROFILE_COUNT) {
        printf("Charge profiles: %u chosen, past the last: the first runs in its place, not stored\n",
               (unsigned) charge_profile_data.chosen);
        charge_profile_data.chosen = 0;
    }
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT; idx += 1) {
        charge_profile_t *profile = &charge_profile_data.profiles[idx];
        if (profile->powder_profile >= powder_count) {
            printf("Charge profiles: %d holds powder %u, past the last: the first runs in its place, not stored\n",
                   idx, (unsigned) profile->powder_profile);
            profile->powder_profile = 0;
        }
        if (!isfinite(profile->target) || profile->target < 0.0f) {
            printf("Charge profiles: %d holds a target that is no weight: not set, not stored\n", idx);
            profile->target = 0.0f;
        }
        profile->name[CHARGE_PROFILE_NAME_LEN - 1] = '\0';
    }
    return found;
}


bool charge_profiles_save(void) {
    return save_config(EEPROM_CHARGE_PROFILES_BASE_ADDR, &charge_profile_data, sizeof(eeprom_charge_profile_data_t));
}


bool charge_profiles_is_saved(void) {
    return config_is_saved(EEPROM_CHARGE_PROFILES_BASE_ADDR, &charge_profile_data,
                           sizeof(eeprom_charge_profile_data_t));
}


uint8_t charge_profiles_chosen(void) {
    return (uint8_t) charge_profile_data.chosen;
}


const charge_profile_t *charge_profiles_get(uint8_t id) {
    return id < CHARGE_PROFILE_COUNT ? &charge_profile_data.profiles[id] : NULL;
}


float charge_profiles_target(void) {
    return charge_profile_data.profiles[charge_profile_data.chosen].target;
}
