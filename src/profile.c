#include <string.h>

#include "profile.h"
#include "eeprom.h"


eeprom_profile_data_t profile_data;

extern void swuart_calcCRC(uint8_t* datagram, uint8_t datagramLength);


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

        // Update default profile data
        for (uint8_t idx=0; idx < MAX_PROFILE_CNT; idx+=1) {
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


bool profile_select(uint8_t idx) {
    profile_data.current_profile_idx = idx;
}


profile_t * get_selected_profile() {
    return &profile_data.profiles[profile_data.current_profile_idx];
}


void profile_update_checksum() {
    swuart_calcCRC((uint8_t *) get_selected_profile(), sizeof(profile_t));
}
