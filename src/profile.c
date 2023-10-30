#include <string.h>

#include "profile.h"
#include "eeprom.h"


eeprom_profile_data_t profile_data;
profile_t current_profile;

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
        profile_data.current_loaded_profile_idx = 0;

        for (int idx = 0; idx < PROFILE_NAME_MAX_LEN; idx += 1) {
            profile_data.profile_addr_offsets[idx] = base_addr + idx * sizeof(profile_t);
        }

        // Write back
        profile_data_save();
    }

    // Load profile
    profile_load(profile_data.current_loaded_profile_idx);

    return true;
}


bool profile_load(uint8_t profile_idx) {
    uint16_t profile_addr = profile_data.profile_addr_offsets[profile_idx];

    bool is_ok = eeprom_read(profile_addr, (uint8_t *) &current_profile, sizeof(profile_t));

    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", profile_addr);
        return false;
    }

    // Verify CRC
    uint8_t read_crc8 = current_profile.crc8;
    swuart_calcCRC((uint8_t *) &current_profile, sizeof(profile_t));

    is_ok = read_crc8 == current_profile.crc8;

    // If the data is either corruped or doesn't exist, assign a new name to the profile
    if (!is_ok) {
        snprintf(current_profile.name, sizeof(current_profile.name), "NewPf%d", profile_idx);
    }

    return is_ok;
}


bool profile_save() {
    // Calculate new CRC
    swuart_calcCRC((uint8_t *) &current_profile, sizeof(profile_t));

    uint16_t target_addr = profile_data.profile_addr_offsets[profile_data.current_loaded_profile_idx];

    bool is_ok = eeprom_write(target_addr,
                              (uint8_t *) &current_profile, 
                              sizeof(profile_t));

    if (!is_ok) {
        printf("Unable to write to EEPROM at address %x\n", target_addr);
        return false;
    }

    is_ok = profile_data_save();

    return is_ok;
}


profile_t * get_current_profile() {
    return &current_profile;
}

uint8_t get_current_profile_idx() {
    return profile_data.current_loaded_profile_idx;
}
