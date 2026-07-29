#include <string.h>
#include <malloc.h>

#include "profile.h"
#include "eeprom.h"
#include "common.h"
#include "charge_mode.h"


#include "profile.h"
#include "eeprom.h"
#include "common.h"
#include "charge_mode.h"
// For backward compatibility: Old profile_t from Rev 0 and 1
typedef struct
{  
    uint32_t rev;
    uint32_t compatibility;
    
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
} profile_v1_t;


typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;

    profile_v1_t profiles[MAX_PROFILE_CNT];
} eeprom_profile_data_v1_t;


eeprom_profile_data_t profile_data;

extern void swuart_calcCRC(uint8_t* datagram, uint8_t datagramLength);

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
        
        .last_charge_weight = 0.0f,
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

        .last_charge_weight = 0.0f,
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

        .last_charge_weight = 0.0f,
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

        .last_charge_weight = 0.0f,
    },
    .profiles[4] = {
        .compatibility = 0,
        .name = "Profile4",
        .last_charge_weight = 0.0f,
    },
    .profiles[5] = {
        .compatibility = 0,
        .name = "Profile5",
        .last_charge_weight = 0.0f,
    },
    .profiles[6] = {
        .compatibility = 0,
        .name = "Profile6",
        .last_charge_weight = 0.0f,
    },
    .profiles[7] = {
        .compatibility = 0,
        .name = "Profile7",
        .last_charge_weight = 0.0f,
    },
};


bool profile_data_save() {
    bool is_ok = save_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, sizeof(profile_data));
    return is_ok;
}


bool profile_data_init() {
    bool is_ok = true;

    // Check for migration from Rev 0 or 1
    size_t size_v1 = sizeof(eeprom_profile_data_v1_t);
    size_t size_v2 = sizeof(eeprom_profile_data_t);
    uint8_t * buf = malloc(size_v1 + 4);
    if (buf) {
        if (eeprom_read(EEPROM_PROFILE_DATA_BASE_ADDR, buf, size_v1 + 4)) {
            uint16_t received_rev;
            memcpy(&received_rev, buf, 2);
            
            bool migrate = false;
            if (received_rev == 1) { // Rev 0 (Legacy)
                migrate = true;
            } else if (received_rev == 0) { // Check if it's Rev 1
                uint32_t calc_crc = software_crc32(buf, size_v1);
                uint32_t rec_crc;
                memcpy(&rec_crc, buf + size_v1, 4);
                if (calc_crc == rec_crc) {
                    migrate = true;
                }
            }
            
            if (migrate) {
                printf("Migrating powder profiles from Rev %d to Rev 2\n", (received_rev == 1 ? 0 : 1));
                eeprom_profile_data_v1_t * old_data = (eeprom_profile_data_v1_t *) buf;
                
                // Initialize new structure with defaults first
                memcpy(&profile_data, &default_profile_data, size_v2);
                
                // Copy old fields
                profile_data.profile_data_rev = 0; // Use CRC path
                profile_data.current_profile_idx = old_data->current_profile_idx;
                
                for (int i = 0; i < MAX_PROFILE_CNT; i++) {
                    // Copy existing fields from the old profile structure
                    memcpy(&profile_data.profiles[i], &old_data->profiles[i], sizeof(profile_v1_t));
                    // Explicitly initialize the new field
                    profile_data.profiles[i].last_charge_weight = 0.0f; 
                }
                
                // Save migrated data (as Rev 2 with CRC)
                save_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, size_v2);
            }
        }
        free(buf);
    }
    // Read profile index table
    is_ok = load_config(EEPROM_PROFILE_DATA_BASE_ADDR, &profile_data, &default_profile_data, sizeof(profile_data), EEPROM_PROFILE_DATA_REV);

    if (is_ok) {
        charge_mode_load_digits_from_profile();
    }

    if (!is_ok) {
        printf("Unable to read profile data\n");
        return false;
    }

    // Register to eeprom save all
    eeprom_register_handler(profile_data_save);

    return true;
}


uint16_t profile_get_selected_idx() {
    uint16_t idx = profile_data.current_profile_idx;
    if (idx >= MAX_PROFILE_CNT) {
        idx = 0;
    }
    return idx;
}


profile_t * profile_get_selected() {
    return &profile_data.profiles[profile_get_selected_idx()];
}


profile_t * profile_select(uint8_t idx) {
    if (idx >= MAX_PROFILE_CNT) {
        idx = 0;
    }
    profile_data.current_profile_idx = idx;

    charge_mode_load_digits_from_profile();
    charge_mode_update_weight_from_profile();

    return profile_get_selected();
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
    // p13 (float): last_charge_weight
    // ee (bool): save to eeprom
    static char buf[512];

    // Read the current loaded profile index
    uint8_t profile_idx = profile_get_selected_idx();

    // Overwrite the profile index (if applicable)
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "pf") == 0) {
            profile_idx = (uint16_t) atoi(values[idx]);
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
                strncpy(current_profile->name, values[idx], sizeof(current_profile->name) - 1);
                current_profile->name[sizeof(current_profile->name) - 1] = '\0';
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
            else if (strcmp(params[idx], "p13") == 0) {
                current_profile->last_charge_weight = strtof(values[idx], NULL);
                charge_mode_update_weight_from_profile();
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
                 "{\"pf\":%d,\"p0\":%ld,\"p1\":%ld,\"p2\":\"%s\",\"p3\":%0.3f,\"p4\":%0.3f,\"p5\":%0.3f,\"p6\":%0.3f,\"p7\":%0.3f,\"p8\":%0.3f,\"p9\":%0.3f,\"p10\":%0.3f,\"p11\":%0.3f,\"p12\":%0.3f,\"p13\":%0.3f}",
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
                 current_profile->fine_max_flow_speed_rps,
                 current_profile->last_charge_weight);
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
    assert(MAX_PROFILE_CNT <= 8);  // Ensures 512 byte buffer us sufficient
    static char buf[512];

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
                 p_idx, profile_data.profiles[p_idx].name);
        char_idx += strnlen((const char *) &buf[char_idx], sizeof(buf) - char_idx);
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