#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/watchdog.h"
#include "hardware/regs/rosc.h"
#include "hardware/regs/addressmap.h"

#include "eeprom.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "common.h"
#include "wireless.h"
#include "app.h"
#include "neopixel_led.h"
#include "rotary_button.h"
#include "version.h"


extern bool cat24c256_eeprom_erase();
extern void cat24c256_eeprom_init();
extern bool cat24c256_write(uint16_t data_addr, uint8_t * data, size_t len);
extern bool cat24c256_read(uint16_t data_addr, uint8_t * data, size_t len);

SemaphoreHandle_t eeprom_access_mutex = NULL;
eeprom_metadata_t metadata;


uint32_t rnd(void){
    int k, random=0;
    volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    
    for(k=0;k<32;k++){
    
    random = random << 1;
    random=random + (0x00000001 & (*rnd_reg));

    }
    return random;
}



uint8_t eeprom_save_all() {
    eeprom_config_save();
    scale_config_save();
    motor_config_save();
    charge_mode_config_save();
    wireless_config_save();
    neopixel_led_config_save();
    button_config_save();
    return 37;  // Configuration Menu ID
}


uint8_t eeprom_erase(bool reboot) {
    cat24c256_eeprom_erase();

    if (reboot) {
        watchdog_reboot(0, 0, 0);
    }
    
    return 37;  // Configuration Menu ID
}


bool eeprom_init(void) {
    bool is_ok = true;
    eeprom_access_mutex = xSemaphoreCreateMutex();

    if (eeprom_access_mutex == NULL) {
        printf("Unable to create EEPROM mutex\n");
        return false;
    }
    
    cat24c256_eeprom_init();

    // Read data revision, if match then move forward
    is_ok = eeprom_read(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_METADATA_BASE_ADDR);
        return false;
    }

    if (metadata.eeprom_metadata_rev != EEPROM_METADATA_REV) {
        // Do some data migration or erase the data
        // printf("EEPROM data revision: %x, Firmware EEPROM data revision: %x, Requires migration\n", metadata.eeprom_metadata_rev, EEPROM_METADATA_REV);

        // Update some data
        metadata.eeprom_metadata_rev = EEPROM_METADATA_REV;

        // Generate id
        snprintf(metadata.unique_id, 8, "%08X", rnd() & 0xffffffff);

        // Write data back
        is_ok = eeprom_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_METADATA_BASE_ADDR);
            return false;
        }
    }

    return is_ok;
}

bool eeprom_config_save() {
    bool is_ok = eeprom_write(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
    return is_ok;
}


static inline void _take_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreTake(eeprom_access_mutex, portMAX_DELAY);
    }
}

static inline void _give_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreGive(eeprom_access_mutex);
    }
}

bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();
    bool is_ok;

    _take_mutex(scheduler_state);

    is_ok = cat24c256_read(data_addr, data, len);

    _give_mutex(scheduler_state);

    return is_ok;
}


bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();
    bool is_ok;

    _take_mutex(scheduler_state);

    is_ok = cat24c256_write(data_addr, data, len);

    _give_mutex(scheduler_state);

    return is_ok;
}


bool eeprom_get_board_id(char ** board_id_buffer, size_t bytes_to_copy) {
    if (bytes_to_copy > sizeof(metadata.unique_id)) {
        exit(-1);
        return false;
    }

    memcpy(board_id_buffer, metadata.unique_id, bytes_to_copy);

    return true;
}


bool http_rest_system_control(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char eeprom_config_json_buffer[256];

    const char * save_to_eeprom_string;
    const char * software_reset_string;
    const char * erase_eeprom_string;

    bool save_to_eeprom_flag = false;
    bool software_reset_flag = false;
    bool erase_eeprom_flag = false;

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "save_to_eeprom") == 0 && strcmp(values[idx], "true") == 0) {
            save_to_eeprom_flag = true;
        }
        else if (strcmp(params[idx], "software_reset") == 0 && strcmp(values[idx], "true") == 0) {
            software_reset_flag = true;
        }
        else if (strcmp(params[idx], "erase_eeprom") == 0 && strcmp(values[idx], "true") == 0) {
            erase_eeprom_flag = true;
        } 
    }

    if (save_to_eeprom_flag) {
        eeprom_save_all();
        save_to_eeprom_string = "true";
    }
    else {
        save_to_eeprom_string = "false";
    }

    if (erase_eeprom_flag) {
        eeprom_erase(software_reset_flag);
        erase_eeprom_string = "true";
    }
    else {
        erase_eeprom_string = "false";
    }

    if (software_reset_flag) {
        software_reboot();
        software_reset_string = "true";
    }
    else {
        software_reset_string = "false";
    }



    snprintf(eeprom_config_json_buffer, 
             sizeof(eeprom_config_json_buffer),
             "{\"unique_id\":\"%s\",\"save_to_eeprom\":%s,\"software_reset\":%s,\"erase_eeprom\":%s,\"ver\":\"%s\",\"hash\":\"%s\",\"dirty\":%s}", 
             metadata.unique_id, save_to_eeprom_string, software_reset_string, erase_eeprom_string,
             version_string, vcs_hash, boolean_string(is_dirty));

    size_t data_length = strlen(eeprom_config_json_buffer);
    file->data = eeprom_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
