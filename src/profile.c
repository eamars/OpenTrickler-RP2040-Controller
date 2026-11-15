#include <math.h>
#include <string.h>

#include "activity.h"
#include "profile.h"
#include "eeprom.h"
#include "common.h"
#include "settings.h"


eeprom_profile_data_t profile_data;

const eeprom_profile_data_t default_profile_data = {
    .profile_data_rev = 0,
    .profiles[0] = {
        .compatibility = 0,
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
        .compatibility = 0,
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
        .compatibility = 0,
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
        .compatibility = 0,
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
        .compatibility = 0,
        .name = "Profile4",
    },
    .profiles[5] = {
        .compatibility = 0,
        .name = "Profile5",
    },
    .profiles[6] = {
        .compatibility = 0,
        .name = "Profile6",
    },
    .profiles[7] = {
        .compatibility = 0,
        .name = "Profile7",
    },
};


bool profile_data_save() {
    bool is_ok = save_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
    return is_ok;
}


bool profile_data_is_saved(void) {
    return config_is_saved(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
}


/*
    A powder profile's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/powder_profiles/{id}). The
    eight are one block in the EEPROM, the one chosen with them, so they share a save. Today's rev and
    compatibility are used by nothing, and are not in the API.
*/
#define PROFILE_FIELD(member) SETTING_AT(profile_t, member)
#define PROFILE_GAIN(field_name, field_label, member, field_group, field_about) \
    {.name = field_name, .label = field_label, .group = field_group, .about = field_about, \
     .type = SETTING_NUMBER, PROFILE_FIELD(member), .min = 0.0f, .max = NAN, .applies = "the next throw"}
#define PROFILE_SPEED(field_name, field_label, member, field_group, field_about) \
    {.name = field_name, .label = field_label, .group = field_group, .about = field_about, \
     .type = SETTING_NUMBER, PROFILE_FIELD(member), .unit = "rev/s", .min = 0.0f, .max = 100.0f, \
     .applies = "the next throw"}

static const setting_field_t profile_fields[] = {
    {.name = "name", .label = "Name", .group = "Profile",
     .about = "The profile's name, as the menu shows it: printable ASCII, the screen's font draws nothing else",
     .type = SETTING_STRING, PROFILE_FIELD(name), .length_min = 1, .applies = "now"},
    PROFILE_GAIN("coarse_kp", "Coarse Kp", coarse_kp, "Coarse trickler", "The coarse trickler's proportional gain"),
    PROFILE_GAIN("coarse_ki", "Coarse Ki", coarse_ki, "Coarse trickler", "The coarse trickler's integral gain"),
    PROFILE_GAIN("coarse_kd", "Coarse Kd", coarse_kd, "Coarse trickler", "The coarse trickler's derivative gain"),
    PROFILE_SPEED("coarse_min_speed_rps", "Coarse minimum speed", coarse_min_flow_speed_rps, "Coarse trickler",
                  "The coarse trickler's slowest while it pours; not above its maximum"),
    PROFILE_SPEED("coarse_max_speed_rps", "Coarse maximum speed", coarse_max_flow_speed_rps, "Coarse trickler",
                  "The coarse trickler's fastest, used up to the coarse motor's max_speed_rps"),
    PROFILE_GAIN("fine_kp", "Fine Kp", fine_kp, "Fine trickler", "The fine trickler's proportional gain"),
    PROFILE_GAIN("fine_ki", "Fine Ki", fine_ki, "Fine trickler", "The fine trickler's integral gain"),
    PROFILE_GAIN("fine_kd", "Fine Kd", fine_kd, "Fine trickler", "The fine trickler's derivative gain"),
    PROFILE_SPEED("fine_min_speed_rps", "Fine minimum speed", fine_min_flow_speed_rps, "Fine trickler",
                  "The fine trickler's slowest while it pours; not above its maximum"),
    PROFILE_SPEED("fine_max_speed_rps", "Fine maximum speed", fine_max_flow_speed_rps, "Fine trickler",
                  "The fine trickler's fastest, used up to the fine motor's max_speed_rps"),
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
    .label = "Powder profile",
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
    bool is_ok = true;

    // Read profile index table
    memset(&profile_data, 0x0, sizeof(eeprom_profile_data_t));
    is_ok = load_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, &default_profile_data, sizeof(profile_data), EEPROM_PROFILE_DATA_REV);

    if (!is_ok) {
        printf("Unable to read profile data\n");
        return false;
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
    return &profile_data.profiles[profile_get_selected_idx()];
}


profile_t * profile_select(uint8_t idx) {
    profile_data.current_profile_idx = idx;

    return profile_get_selected(idx);
}


bool http_rest_profile_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // pf (int): profile index
    // p0 (int): rev
    // p1 (int): compatibility
    // p2 (str): name
    // p3 (float): coarse_kp
    // p4 (float): coarse_ki
    // p5 (float): coarse_kd
    // p6 (float): coarse_min_flow_speed_rps
    // p7 (float): coarse_max_flow_speed_rps
    // p8 (float): fine_kp
    // p9 (float): fine_ki
    // p10 (float): fine_kd
    // p11 (float): fine_min_flow_speed_rps
    // p12 (float): fine_max_flow_speed_rps
    // ee (bool): save to eeprom
    static char buf[256];

    // Read the current loaded profile index
    uint8_t profile_idx = profile_get_selected_idx();

    // Overwrite the profile index (if applicable)
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "pf") == 0) {
            profile_idx = (uint16_t) atoi(values[0]);
        }
    }

    if (profile_idx >= MAX_PROFILE_CNT) {
        strcpy(buf, "{\"error\":\"InvalidProfileIndex\"}");
    }

    else {
        profile_t * current_profile = profile_select(profile_idx);
        bool save_to_eeprom = false;

        // Control
        for (int idx = 0; idx < num_params; idx += 1) {
            if (strcmp(params[idx], "p0") == 0) {
                current_profile->rev = strtol(values[idx], NULL, 10);
            }
            else if (strcmp(params[idx], "p1") == 0) {
                current_profile->compatibility = strtol(values[idx], NULL, 10);
            }
            else if (strcmp(params[idx], "p2") == 0) {
                strncpy(current_profile->name, values[idx], sizeof(current_profile->name));
            }
            else if (strcmp(params[idx], "p3") == 0) {
                current_profile->coarse_kp = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p4") == 0) {
                current_profile->coarse_ki = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p5") == 0) {
                current_profile->coarse_kd = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p6") == 0) {
                current_profile->coarse_min_flow_speed_rps = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p7") == 0) {
                current_profile->coarse_max_flow_speed_rps = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p8") == 0) {
                current_profile->fine_kp = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p9") == 0) {
                current_profile->fine_ki = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p10") == 0) {
                current_profile->fine_kd = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p11") == 0) {
                current_profile->fine_min_flow_speed_rps = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "p12") == 0) {
                current_profile->fine_max_flow_speed_rps = strtof(values[idx], NULL);
            }
            else if (strcmp(params[idx], "ee") == 0) {
                save_to_eeprom = string_to_boolean(values[idx]);
            }
        }

        // Perform action
        if (save_to_eeprom) {
            profile_data_save();
        }

        // Response
        snprintf(buf, sizeof(buf), 
                 "%s"
                 "{\"pf\":%d,\"p0\":%ld,\"p1\":%ld,\"p2\":\"%s\",\"p3\":%0.3f,\"p4\":%0.3f,\"p5\":%0.3f,\"p6\":%0.3f,\"p7\":%0.3f,\"p8\":%0.3f,\"p9\":%0.3f,\"p10\":%0.3f,\"p11\":%0.3f,\"p12\":%0.3f}",
                 http_json_header,
                 profile_idx, 
                 current_profile->rev,
                 current_profile->compatibility,
                 current_profile->name,
                 current_profile->coarse_kp,
                 current_profile->coarse_ki,
                 current_profile->coarse_kd,
                 current_profile->coarse_min_flow_speed_rps,
                 current_profile->coarse_max_flow_speed_rps,
                 current_profile->fine_kp,
                 current_profile->fine_ki,
                 current_profile->fine_kd,
                 current_profile->fine_min_flow_speed_rps,
                 current_profile->fine_max_flow_speed_rps);
    }

    size_t response_len = strlen(buf);
    file->data = buf;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


