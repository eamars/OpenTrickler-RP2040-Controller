#include <string.h>

#include "hardware/watchdog.h"

#include "system_control.h"
#include "common.h"
#include "eeprom.h"
#include "version.h"

extern eeprom_metadata_t metadata;


int software_reboot() {
    watchdog_reboot(0, 0, 0);

    return 0;
}



bool http_rest_system_control(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // s0 (str): unique_id
    // s1 (str): version_string
    // s2 (str): vcs_hash
    // s3 (str): build_type
    // s4 (bool): save_to_eeprom
    // s5 (bool): software_reset
    // s6 (bool): erase_eeprom
    static char eeprom_config_json_buffer[256];

    bool save_to_eeprom_flag = false;
    bool software_reset_flag = false;
    bool erase_eeprom_flag = false;

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "s4") == 0) {
            save_to_eeprom_flag = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "s5") == 0) {
            software_reset_flag = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "s6") == 0) {
            erase_eeprom_flag = string_to_boolean(values[idx]);
        } 
    }

    if (save_to_eeprom_flag) {
        eeprom_save_all();
    }

    if (erase_eeprom_flag) {
        eeprom_erase(software_reset_flag);
    }

    if (software_reset_flag) {
        software_reboot();
    }

    // Response
    snprintf(eeprom_config_json_buffer, 
             sizeof(eeprom_config_json_buffer),
             "%s"
             "{\"s0\":\"%s\",\"s1\":\"%s\",\"s2\":\"%s\",\"s3\":\"%s\",\"s4\":%s,\"s5\":%s,\"s6\":%s}", 
             http_json_header,
             metadata.unique_id, version_string, vcs_hash, build_type,
             boolean_to_string(save_to_eeprom_flag),
             boolean_to_string(erase_eeprom_flag),
             boolean_to_string(software_reset_flag));

    size_t data_length = strlen(eeprom_config_json_buffer);
    file->data = eeprom_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
