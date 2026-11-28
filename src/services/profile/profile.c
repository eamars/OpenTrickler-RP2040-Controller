#include <math.h>
#include <stdio.h>
#include <string.h>

#include "activity.h"
#include "profile.h"
#include "eeprom.h"
#include "common.h"
#include "settings.h"


eeprom_profile_data_t profile_data;
// The powder in use: the chosen charge profile's (ADR-014), in RAM. The record's current_profile_idx stays as stored
static uint16_t profile_in_use = 0;

const eeprom_profile_data_t default_profile_data = {
    .profile_data_rev = 0,
    .profiles[0] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "AR2208,gr",

        .coarse_kp = 0.025f,
        .coarse_ki = 0.0f,
        .coarse_kd = 0.3f,
        .coarse_min_flow_speed_rps = 0.1f,
        .coarse_max_flow_speed_rps = 5.0f,

        .fine_kp = 2.0f,
        .fine_ki = 0.0f,
        .fine_kd = 10.0f,
        .fine_min_flow_speed_rps = 0.08f,
        .fine_max_flow_speed_rps = 5.0f,
    },
    .profiles[1] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "AR2209,gr",

        .coarse_kp = 0.05f,
        .coarse_ki = 0.0f,
        .coarse_kd = 0.3f,
        .coarse_min_flow_speed_rps = 0.1f,
        .coarse_max_flow_speed_rps = 8.0f,

        .fine_kp = 0.8f,
        .fine_ki = 0.0f,
        .fine_kd = 15.0f,
        .fine_min_flow_speed_rps = 0.08f,
        .fine_max_flow_speed_rps = 2.0f,
    },
    .profiles[2] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "8208XBR,gr",

        .coarse_kp = 0.05f,
        .coarse_ki = 0.0f,
        .coarse_kd = 0.3f,
        .coarse_min_flow_speed_rps = 0.1f,
        .coarse_max_flow_speed_rps = 5.0f,

        .fine_kp = 2.0f,
        .fine_ki = 0.0f,
        .fine_kd = 12.0f,
        .fine_min_flow_speed_rps = 0.06f,
        .fine_max_flow_speed_rps = 5.0f,
    },
    .profiles[3] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Benchmark2,gr",

        .coarse_kp = 0.06f,
        .coarse_ki = 0.0f,
        .coarse_kd = 0.3f,
        .coarse_min_flow_speed_rps = 0.1f,
        .coarse_max_flow_speed_rps = 5.0f,

        .fine_kp = 0.8f,
        .fine_ki = 0.0f,
        .fine_kd = 15.0f,
        .fine_min_flow_speed_rps = 0.08f,
        .fine_max_flow_speed_rps = 5.0f,
    },
    .profiles[4] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile4",
    },
    .profiles[5] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile5",
    },
    .profiles[6] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile6",
    },
    .profiles[7] = {
        .compatibility = 0, .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile7",
    },
};


bool profile_data_save() {
    bool is_ok = save_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(eeprom_profile_data_t));
    return is_ok;
}


bool profile_data_is_saved(void) {
    return config_is_saved(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
}


/*
    A powder profile's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/powder_profiles/{id}). The
    eight are one block in the EEPROM, so they share a save. Today's rev and compatibility are used by nothing, and
    are not in the API; nor is current_profile_idx, the choice a version before ADR-014 stored there.
*/
#define PROFILE_FIELD(member) SETTING_AT(profile_t, member)
#define PROFILE_GAIN(field_name, field_label, member, field_about) \
    {.name = field_name, .label = field_label, .about = field_about, \
     .type = SETTING_NUMBER, PROFILE_FIELD(member), .min = 0.0f, .max = NAN, .applies = "the next throw"}
#define PROFILE_SPEED(field_name, field_label, member, field_about) \
    {.name = field_name, .label = field_label, .about = field_about, \
     .type = SETTING_NUMBER, PROFILE_FIELD(member), .unit = "rev/s", .min = 0.0f, .max = 100.0f, \
     .applies = "the next throw"}

static const setting_field_t profile_fields[] = {
    {.name = "name", .label = "Name",
     .about = "The powder's name, as the menu shows it: printable ASCII, the screen's font draws nothing else",
     .type = SETTING_STRING, PROFILE_FIELD(name), .length_min = 1, .applies = "now"},
    PROFILE_GAIN("coarse_kp", "Coarse Kp", coarse_kp, "The coarse motor's proportional gain"),
    PROFILE_GAIN("coarse_ki", "Coarse Ki", coarse_ki, "The coarse motor's integral gain"),
    PROFILE_GAIN("coarse_kd", "Coarse Kd", coarse_kd, "The coarse motor's derivative gain"),
    PROFILE_SPEED("coarse_min_speed_rps", "Coarse minimum speed", coarse_min_flow_speed_rps,
                  "The coarse motor's slowest while it pours; not above its maximum"),
    PROFILE_SPEED("coarse_max_speed_rps", "Coarse maximum speed", coarse_max_flow_speed_rps,
                  "The coarse motor's fastest while it pours; not above its Maximum speed in Settings, Motors"),
    PROFILE_GAIN("fine_kp", "Fine Kp", fine_kp, "The fine motor's proportional gain"),
    PROFILE_GAIN("fine_ki", "Fine Ki", fine_ki, "The fine motor's integral gain"),
    PROFILE_GAIN("fine_kd", "Fine Kd", fine_kd, "The fine motor's derivative gain"),
    PROFILE_SPEED("fine_min_speed_rps", "Fine minimum speed", fine_min_flow_speed_rps,
                  "The fine motor's slowest while it pours; not above its maximum"),
    PROFILE_SPEED("fine_max_speed_rps", "Fine maximum speed", fine_max_flow_speed_rps,
                  "The fine motor's fastest while it pours; not above its Maximum speed in Settings, Motors"),
    // ADR-009 step 2: the powder's calibration measures it; the charge's first throw on this powder forecasts from it
    {.name = "fine_weight_per_turn", .label = "Fine weight a turn", .group = "Calibration",
     .about = "The fine tube's weight a turn where it streams, kept by the powder's calibration: the charge forecasts "
              "its first throws on this powder from it, until they give their own; null, never calibrated",
     .type = SETTING_NUMBER, PROFILE_FIELD(fine_weight_per_turn), .unit = "weight", .min = 0.001f, .max = 100.0f,
     .nullable = true, .null_held = 0.0f, .applies = "the next throw"},
    {.name = "coarse_weight_per_turn", .label = "Coarse weight a turn", .group = "Calibration",
     .about = "The coarse tube's weight a turn where it streams, kept by the powder's calibration; null, never "
              "calibrated",
     .type = SETTING_NUMBER, PROFILE_FIELD(coarse_weight_per_turn), .unit = "weight", .min = 0.001f, .max = 100.0f,
     .nullable = true, .null_held = 0.0f, .applies = "the next throw"},
};


static void *profile_settings_live(int32_t id) {
    return &profile_data.profiles[id];
}


static const char *profile_settings_check(const void *proposed, int32_t id, const char **field) {
    (void) id;
    const profile_t *profile = proposed;
    for (size_t idx = 0; idx < sizeof(profile->name) && profile->name[idx] != '\0'; idx += 1) {
        if (profile->name[idx] < 0x20 || profile->name[idx] > 0x7e) {
            *field = "name";
            return "name is printable ASCII only";
        }
    }
    if (profile->coarse_min_flow_speed_rps > profile->coarse_max_flow_speed_rps) {
        *field = "coarse_min_speed_rps";
        return "Coarse minimum speed is above its maximum";
    }
    if (profile->fine_min_flow_speed_rps > profile->fine_max_flow_speed_rps) {
        *field = "fine_min_speed_rps";
        return "Fine minimum speed is above its maximum";
    }
    return NULL;
}


// The profile in use is read by the throw: not written while one is under way. The others are free
static const char *profile_settings_refuse(int32_t id) {
    if (activity_in_throw() && id == profile_in_use) {
        return "the profile in use is refused while a throw is under way";
    }
    return NULL;
}


static const settings_resource_t profile_settings = {
    .path = "/api/v1/powder_profiles/{id}",
    .save_path = "/api/v1/powder_profiles/save",
    .revert_path = "/api/v1/powder_profiles/revert",
    .addr = EEPROM_PROFILE_DATA_BASE_ADDR + offsetof(eeprom_profile_data_t, profiles),
    .label = "Powder",
    .about = "How a powder flows, and the controller's gains for it",
    SETTING_FIELDS(profile_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(profile_t),
    .id_count = MAX_PROFILE_CNT,
    .live = profile_settings_live,
    .check = profile_settings_check,
    .refuse = profile_settings_refuse,
    .save = profile_data_save,
    .is_saved = profile_data_is_saved,
};


const char *profile_get_name(uint16_t idx) {
    static char name[PROFILE_NAME_MAX_LEN + 1];
    if (idx >= MAX_PROFILE_CNT) {
        return NULL;
    }
    // A name of 16 was stored with no ending before gate 3
    size_t len = strnlen(profile_data.profiles[idx].name, PROFILE_NAME_MAX_LEN);
    memcpy(name, profile_data.profiles[idx].name, len);
    name[len] = '\0';
    return name;
}


bool profile_data_init() {
    // Read profile index table
    memset(&profile_data, 0x0, sizeof(eeprom_profile_data_t));
    // A header and eight powders: each powder of an earlier or a later version read for the fields it has
    static const config_layout_t layout = {.header = offsetof(eeprom_profile_data_t, profiles),
                                           .element = sizeof(profile_t), .count = MAX_PROFILE_CNT};
    // Whatever is found, the struct holds settings to run on (ADR-017, gate 1)
    load_config_layout(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, &default_profile_data,
                       sizeof(eeprom_profile_data_t), &layout);

    // Until the charge profiles start (charge_profiles_init), the powder the unit had chosen
    profile_in_use = profile_get_stored_choice();

    settings_register(&profile_settings);

    return true;
}


uint16_t profile_get_stored_choice(void) {
    return profile_data.current_profile_idx < MAX_PROFILE_CNT ? profile_data.current_profile_idx : 0;
}


uint16_t profile_get_selected_idx() {
    return profile_in_use;
}


profile_t * profile_get_selected() {
    return &profile_data.profiles[profile_get_selected_idx()];
}


profile_t * profile_get(uint16_t idx) {
    return idx < MAX_PROFILE_CNT ? &profile_data.profiles[idx] : NULL;
}


profile_t * profile_select(uint8_t idx) {
    if (idx < MAX_PROFILE_CNT) {
        profile_in_use = idx;
    }

    return profile_get_selected();
}


