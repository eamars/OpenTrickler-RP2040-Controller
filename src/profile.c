#include <string.h>

#include "profile.h"
#include "eeprom.h"
#include "common.h"


eeprom_profile_data_t profile_data;

extern void swuart_calcCRC(uint8_t* datagram, uint8_t datagramLength);


const profile_t default_ar_2208_profile = {
    .rev = 0,
    .compatibility = 0,

    .name = "AR2208,gr",

    .coarse_kp = 0.025f,
    .coarse_ki = 0.0f,
    .coarse_kd = 0.25f,
    .coarse_min_flow_speed_rps = 0.1f,
    .coarse_max_flow_speed_rps = 5.0f,

    .fine_kp = 2.0f,
    .fine_ki = 0.0f,
    .fine_kd = 10.0f,
    .fine_min_flow_speed_rps = 0.1f,
    .fine_max_flow_speed_rps = 3.0f,
};


const profile_t default_ar_2209_profile = {
    .rev = 0,
    .compatibility = 0,

    .name = "AR2209,gr",

    .coarse_kp = 0.02f,
    .coarse_ki = 0.0f,
    .coarse_kd = 0.30f,
    .coarse_min_flow_speed_rps = 0.1f,
    .coarse_max_flow_speed_rps = 5.0f,

    .fine_kp = 2.0f,
    .fine_ki = 0.0f,
    .fine_kd = 10.0f,

    .fine_min_flow_speed_rps = 0.08f,
    .fine_max_flow_speed_rps = 5.0f,
};




bool profile_data_save() {
    bool is_ok = eeprom_write(EEPROM_PROFILE_DATA_BASE_ADDR, (uint8_t *) &profile_data, sizeof(eeprom_profile_data_t));
    if (!is_ok) {
        printf("Unable to write to EEPROM at address %x\n", EEPROM_PROFILE_DATA_BASE_ADDR);
        return false;
    }

    return true;
}


bool profile_data_init() {
    bool is_ok = true;

    // Read profile index table
    memset(&profile_data, 0x0, sizeof(eeprom_profile_data_t));
    is_ok = eeprom_read(EEPROM_PROFILE_DATA_BASE_ADDR, (uint8_t *) &profile_data, sizeof(eeprom_profile_data_t));

    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_PROFILE_DATA_BASE_ADDR);
        return false;
    }

    if (profile_data.profile_data_rev != EEPROM_PROFILE_DATA_REV) {
        profile_data.profile_data_rev = EEPROM_PROFILE_DATA_REV;
        // Set default selected profile
        profile_data.current_profile_idx = 0;

        // Reset all profiles
        memset(profile_data.profiles, 0x0, sizeof(profile_data.profiles));

        // Copy two default profiles
        memcpy(&profile_data.profiles[0], &default_ar_2208_profile, sizeof(profile_t));
        memcpy(&profile_data.profiles[1], &default_ar_2209_profile, sizeof(profile_t));

        // Update default profile data
        for (uint8_t idx=2; idx < MAX_PROFILE_CNT; idx+=1) {
            profile_t * selected_profile = &profile_data.profiles[idx];

            // Provide default name
            snprintf(selected_profile->name, PROFILE_NAME_MAX_LEN, 
                     "NewProfile%d", idx);
        }

        // Write back
        profile_data_save();
    }

    // Register to eeprom save all
    eeprom_register_handler(profile_data_save);

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


void profile_update_checksum() {
    swuart_calcCRC((uint8_t *) profile_get_selected(), sizeof(profile_t));
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


bool http_rest_profile_summary(struct fs_file *file, int num_params, char *params[], char *values[])
{
    // It does not take argument
    assert(MAX_PROFILE_CNT <= 8);  // Ensures 256 byte buffer us sufficient
    static char buf[256];

    // Response
    // s0 (dict): A dictionary of all profiles in {idx: name} format. 
    // s1 (int): The current loaded profile index
    memset(buf, 0x0, sizeof(buf));
    const char * item_template = "\"%d\":\"%s\",";

    // Create header
    snprintf(buf, sizeof(buf), 
             "%s{\"s0\":{",
             http_json_header);

    size_t char_idx = strlen(buf);

    // Write profile information
    for (uint8_t p_idx=0; p_idx < MAX_PROFILE_CNT; p_idx+=1) {
        snprintf(&buf[char_idx], sizeof(buf) - char_idx, 
                 item_template,
                 p_idx, &profile_data.profiles[p_idx].name);
        char_idx += strnlen((const char *) &buf[char_idx], sizeof(buf));
    }

    // Append close bracket (replace the last comma)
    buf[char_idx - 1] = '}';

    // Append s1
    snprintf(&buf[char_idx], sizeof(buf) - char_idx,
             ",\"s1\":%d}", 
             profile_data.current_profile_idx);

    size_t response_len = strlen(buf);
    file->data = buf;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}