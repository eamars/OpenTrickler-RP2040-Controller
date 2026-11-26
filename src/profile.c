#include <math.h>
#include <stdio.h>
#include <string.h>

#include "activity.h"
#include "profile.h"
#include "eeprom.h"
#include "common.h"
#include "settings.h"


// The powder chosen, and the eight: each a record of its own (ADR-009, 2026-11-26)
static eeprom_profile_data_t profile_data;
static profile_t profiles[MAX_PROFILE_CNT];

static const eeprom_profile_data_t default_profile_data = {
    .profile_data_rev = 0,
    .current_profile_idx = 0,
};

static const profile_t default_profiles[MAX_PROFILE_CNT] = {
    [0] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
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
    [1] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
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
    [2] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
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
    [3] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
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
    [4] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile4",
    },
    [5] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile5",
    },
    [6] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile6",
    },
    [7] = {
        .fine_weight_per_turn = 0.0f, .coarse_weight_per_turn = 0.0f,
        .name = "Profile7",
    },
};


static uint16_t profile_addr(uint16_t idx) {
    return (uint16_t) (EEPROM_POWDER_BASE_ADDR + idx * EEPROM_SLOT_SIZE);
}


// The choice and every powder that differs from what is stored: one save for the nine records
bool profile_data_save() {
    bool is_ok = true;
    if (!config_is_saved(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data))) {
        is_ok = save_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
    }
    for (uint16_t idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        if (!config_is_saved(profile_addr(idx), &profiles[idx], sizeof(profile_t))) {
            is_ok = save_config(profile_addr(idx), &profiles[idx], sizeof(profile_t)) && is_ok;
        }
    }
    return is_ok;
}


bool profile_data_is_saved(void) {
    bool saved = config_is_saved(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
    for (uint16_t idx = 0; idx < MAX_PROFILE_CNT && saved; idx += 1) {
        saved = config_is_saved(profile_addr(idx), &profiles[idx], sizeof(profile_t));
    }
    return saved;
}


/*
    A powder profile's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/powder_profiles/{id}). Each
    powder is a record of its own, a slot apart; the eight and the choice share a save. Today's rev is used by
    nothing, and is not in the API.
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
    return &profiles[id];
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
        return "coarse_min_speed_rps is above coarse_max_speed_rps";
    }
    if (profile->fine_min_flow_speed_rps > profile->fine_max_flow_speed_rps) {
        *field = "fine_min_speed_rps";
        return "fine_min_speed_rps is above fine_max_speed_rps";
    }
    return NULL;
}


// The profile in use is read by the throw: not written while one is under way. The others are free
static const char *profile_settings_refuse(int32_t id) {
    if (activity_in_throw() && id == profile_data.current_profile_idx) {
        return "the profile in use is refused while a throw is under way";
    }
    return NULL;
}


static const settings_resource_t profile_settings = {
    .path = "/api/v1/powder_profiles/{id}",
    .save_path = "/api/v1/powder_profiles/save",
    .revert_path = "/api/v1/powder_profiles/revert",
    .addr = EEPROM_POWDER_BASE_ADDR,
    .stride = EEPROM_SLOT_SIZE,
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
    size_t len = strnlen(profiles[idx].name, PROFILE_NAME_MAX_LEN);
    memcpy(name, profiles[idx].name, len);
    name[len] = '\0';
    return name;
}


bool profile_data_init() {
    bool is_ok = true;

    // The choice, then each powder from its own record
    memset(&profile_data, 0x0, sizeof(eeprom_profile_data_t));
    is_ok = load_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, &default_profile_data, sizeof(profile_data), EEPROM_PROFILE_DATA_REV);
    for (uint16_t idx = 0; idx < MAX_PROFILE_CNT && is_ok; idx += 1) {
        is_ok = load_config(profile_addr(idx), &profiles[idx], &default_profiles[idx], sizeof(profile_t),
                            PROFILE_RECORD_REV);
    }

    if (!is_ok) {
        printf("Unable to read profile data\n");
        return false;
    }
    if (profile_data.current_profile_idx >= MAX_PROFILE_CNT) {
        profile_data.current_profile_idx = 0;
    }

    // Register to eeprom save all
    eeprom_register_handler(profile_data_save);
    settings_register(&profile_settings);

    return true;
}


uint16_t profile_get_selected_idx() {
    return profile_data.current_profile_idx;
}


profile_t * profile_get_selected() {
    return &profiles[profile_get_selected_idx()];
}


profile_t * profile_get(uint16_t idx) {
    return idx < MAX_PROFILE_CNT ? &profiles[idx] : NULL;
}


profile_t * profile_select(uint8_t idx) {
    profile_data.current_profile_idx = idx;

    return profile_get_selected(idx);
}


