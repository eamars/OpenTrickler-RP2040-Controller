#include <string.h>

#include "profile.h"
#include "eeprom.h"


eeprom_profile_data_t profile_data;

extern void swuart_calcCRC(uint8_t* datagram, uint8_t datagramLength);


const profile_t default_ar_2208_profile = {
    .rev = 0,
    .compatibility = 0,

    .name = "AR2208,gr",

    .coarse_kp = 0.025f,
    .coarse_ki = 0.0f,
    .coarse_kd = 0.25f,

    .fine_kp = 2.0f,
    .fine_ki = 0.0f,
    .fine_kd = 10.0f,

    .min_flow_speed_rps = 0.1,
    .max_flow_speed_rps = 5
};


const profile_t default_ar_2209_profile = {
    .rev = 0,
    .compatibility = 0,

    .name = "AR2209,gr",

    .coarse_kp = 0.02f,
    .coarse_ki = 0.0f,
    .coarse_kd = 0.30f,

    .fine_kp = 2.0f,
    .fine_ki = 0.0f,
    .fine_kd = 10.0f,

    .min_flow_speed_rps = 0.08,
    .max_flow_speed_rps = 5
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
        // Generate new set of data
        uint16_t base_addr = EEPROM_PROFILE_DATA_BASE_ADDR + sizeof(eeprom_profile_data_t);

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

            // Generate new CRC
            swuart_calcCRC((uint8_t *) selected_profile, sizeof(profile_t));
        }

        // Write back
        profile_data_save();
    }

    return true;
}


profile_t * profile_select(uint8_t idx) {
    profile_data.current_profile_idx = idx;

    return get_selected_profile(idx);
}


profile_t * get_selected_profile() {
    return &profile_data.profiles[profile_data.current_profile_idx];
}


void profile_update_checksum() {
    swuart_calcCRC((uint8_t *) get_selected_profile(), sizeof(profile_t));
}

bool http_rest_profile_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // pf (int): profile index
    // p0 (int): rev
    // p1 (int): compatibility
    // p2 (str): name

    const char * error_msg = NULL;
    if (num_params < 1 && strcmp(params[0], "pf") != 0) {
        error_msg = "incorrect_profile_index";
    }
    else {
        // Read profile index as the first parameter
        uint8_t profile_idx = strtod(values[0], NULL);

        profile_t * current_profile = profile_select(profile_idx);

        // Control
        for (int idx = 1; idx < num_params; idx += 1) {
            if (strcmp(params[idx], "p0")) {
                current_profile->rev = strtol(values[idx], NULL, 10);
            }
            else if (strcmp(params[idx], "p1")) {
                current_profile->compatibility = strtol(values[idx], NULL, 10);
            }
            else if (strcmp(params[idx], "p2") == 0) {
                strncpy(current_profile->name, values[idx], sizeof(current_profile->name));
            }
    }
}