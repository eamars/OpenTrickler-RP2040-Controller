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
    eeprom_save_config();
    scale_config_save();
    motor_config_save();
    charge_mode_config_save();
    wireless_config_save();
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
        is_ok = eeprom_write(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_METADATA_BASE_ADDR);
            return false;
        }
    }

    return is_ok;
}

bool eeprom_save_config() {
    bool is_ok;

    is_ok = eeprom_write(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
    if (!is_ok) {
        printf("Unable to write to %x\n", EEPROM_METADATA_BASE_ADDR);
        return false;
    }

    return true;
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


char * eeprom_config_to_json() {
    static char eeprom_config_json_buffer[64];
    sprintf(eeprom_config_json_buffer, 
            "{\"unique_id\":\"%s\"}", 
            metadata.unique_id);

    return eeprom_config_json_buffer;
}