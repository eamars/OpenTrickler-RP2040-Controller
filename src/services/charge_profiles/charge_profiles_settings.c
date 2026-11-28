/*
 * The charge profiles' settings, described once (ADR-014; api_v1.md, /api/v1/charge_profiles/{id}), and the writes
 * the charge, the screen and the API share. The eight are one block in the EEPROM, the one chosen with them, so they
 * share a save and a revert
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "activity.h"
#include "charge_profiles.h"
#include "charge_profiles_record.h"
#include "common.h"
#include "eeprom.h"
#include "profile.h"
#include "settings.h"


static bool (*charge_profile_target_fits)(float target) = NULL;


void charge_profiles_set_target_check(bool (*fits)(float target)) {
    charge_profile_target_fits = fits;
}


#define CHARGE_PROFILE_FIELD(member) SETTING_AT(charge_profile_t, member)

static const setting_field_t charge_profile_fields[] = {
    {.name = "name", .label = "Name",
     .about = "The load's name, as the menu shows it: printable ASCII, the screen's font draws nothing else. A unit "
              "of weight, if wanted, goes in the name",
     .type = SETTING_STRING, CHARGE_PROFILE_FIELD(name), .length_min = 1, .applies = "now"},
    {.name = "target", .label = "Target",
     .about = "The weight thrown, in the scale's unit: one the screen's digits show at the charge's decimal places, "
              "to 999.99 at two and to 99.999 at three; null, not set",
     .type = SETTING_NUMBER, CHARGE_PROFILE_FIELD(target), .unit = "weight", .min = 0.001f, .max = 999.99f,
     .nullable = true, .null_held = 0.0f, .applies = "the next throw"},
    {.name = "powder_profile", .label = "Powder",
     .about = "The powder it is thrown with, by its powder profile's id",
     .type = SETTING_INTEGER, CHARGE_PROFILE_FIELD(powder_profile), .min = 0.0f, .max = MAX_PROFILE_CNT - 1,
     .applies = "the next throw"},
};


static void *charge_profile_live(int32_t id) {
    return &charge_profiles_record()->profiles[id];
}


// A write's value, and what is stored as a revert gives it back
static const char *charge_profile_check(const void *proposed, int32_t id, const char **field) {
    (void) id;
    const charge_profile_t *profile = proposed;
    if (profile->name[CHARGE_PROFILE_NAME_LEN - 1] != '\0') {
        *field = "name";
        return "name is longer than 15 bytes";
    }
    for (size_t idx = 0; profile->name[idx] != '\0'; idx += 1) {
        if (profile->name[idx] < 0x20 || profile->name[idx] > 0x7e) {
            *field = "name";
            return "name is printable ASCII only";
        }
    }
    bool weight = profile->target >= 0.0f && profile->target <= 999.99f;     // false for what is no number
    if (!weight || (profile->target > 0.0f && charge_profile_target_fits != NULL &&
                    !charge_profile_target_fits(profile->target))) {
        *field = "target";
        return "target is not a weight the screen's digits show at the decimal places set";
    }
    if (profile->powder_profile >= MAX_PROFILE_CNT) {
        *field = "powder_profile";
        return "no powder profile has that id";
    }
    return NULL;
}


// The chosen one's powder is the powder in use
static void charge_profile_apply(const void *before, int32_t id) {
    (void) before;
    eeprom_charge_profile_data_t *data = charge_profiles_record();
    if (id == data->chosen) {
        profile_select(data->profiles[id].powder_profile);
    }
}


// The chosen one is read by the throw: not written while one is under way. The others are free
static const char *charge_profile_refuse(int32_t id) {
    if (activity_in_throw() && id == charge_profiles_record()->chosen) {
        return "the charge profile in use is refused while a throw is under way";
    }
    return NULL;
}


/*
    The choice lies in the block's header, outside the eight parts a revert puts back: put back here. A chosen index no
    profile has is read as the load reads it, the first
*/
static const char *charge_profile_revert_choice(bool write) {
    eeprom_charge_profile_data_t *data = charge_profiles_record();
    uint16_t stored;
    if (!config_read_part(EEPROM_CHARGE_PROFILES_BASE_ADDR + offsetof(eeprom_charge_profile_data_t, chosen), &stored,
                          sizeof(stored))) {
        return NULL;    // not read: its parts did not read either, and the revert said so
    }
    stored = stored < CHARGE_PROFILE_COUNT ? stored : 0;
    if (stored == data->chosen) {
        return NULL;
    }
    if (!write) {
        return activity_in_throw() ? "the charge profile in use is refused while a throw is under way" : NULL;
    }
    data->chosen = stored;
    profile_select(data->profiles[stored].powder_profile);
    return NULL;
}


static const settings_resource_t charge_profile_settings = {
    .path = "/api/v1/charge_profiles/{id}",
    .save_path = "/api/v1/charge_profiles/save",
    .revert_path = "/api/v1/charge_profiles/revert",
    .addr = EEPROM_CHARGE_PROFILES_BASE_ADDR + offsetof(eeprom_charge_profile_data_t, profiles),
    .label = "Load",
    .about = "What is thrown: a name, a target and the powder it is thrown with",
    SETTING_FIELDS(charge_profile_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(charge_profile_t),
    .id_count = CHARGE_PROFILE_COUNT,
    .live = charge_profile_live,
    .check = charge_profile_check,
    .apply = charge_profile_apply,
    .refuse = charge_profile_refuse,
    .revert_rest = charge_profile_revert_choice,
    .save = charge_profiles_save,
    .is_saved = charge_profiles_is_saved,
};


bool charge_profiles_init(void) {
#if RC_TEST_LOADS_BLANK
    // A test build (ADR-014): the loads' slot and the spare slot blank before the load, as on a unit that never ran a
    // version with loads (one updated from v1.21): the spare would otherwise finish the slot from the loads' last save
    static const uint8_t blank[2] = {0xff, 0xff};
    eeprom_write(EEPROM_CHARGE_PROFILES_BASE_ADDR, (uint8_t *) blank, sizeof(blank));
    eeprom_write(EEPROM_SPARE_BASE_ADDR, (uint8_t *) blank, sizeof(blank));
    printf("Charge profiles: test build, their slot and the spare made blank\n");
#endif
    charge_profiles_load(profile_get_stored_choice(), MAX_PROFILE_CNT);
    profile_select(charge_profiles_get(charge_profiles_chosen())->powder_profile);
    return settings_register(&charge_profile_settings);
}


const char *charge_profiles_choose(uint8_t id) {
    if (id >= CHARGE_PROFILE_COUNT) {
        return "no charge profile has that id";
    }
    eeprom_charge_profile_data_t *data = charge_profiles_record();
    activity_lock();
    bool in_throw = activity_in_throw();
    if (!in_throw) {
        data->chosen = id;
        profile_select(data->profiles[id].powder_profile);
    }
    activity_unlock();
    return in_throw ? "not while a throw pours" : NULL;
}


// A field of the chosen profile, written through the settings' table. Under the lock, so the choice stays
static const char *charge_profile_write(const char *name, const char *value, activity_by_t by) {
    char path[32];
    activity_lock();
    snprintf(path, sizeof(path), "charge_profiles/%u", (unsigned) charge_profiles_chosen());
    const char *why = settings_write_by(path, name, value, by);
    activity_unlock();
    return why;
}


const char *charge_profiles_set_target(float target, activity_by_t by) {
    char text[16];
    snprintf(text, sizeof(text), "%.3f", (double) target);
    return charge_profile_write("target", text, by);
}


const char *charge_profiles_set_powder(uint8_t powder_profile, activity_by_t by) {
    char text[8];
    snprintf(text, sizeof(text), "%u", (unsigned) powder_profile);
    return charge_profile_write("powder_profile", text, by);
}
