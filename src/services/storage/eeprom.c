#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cat24c256_eeprom.h"
#include "eeprom.h"
#include "common.h"


SemaphoreHandle_t eeprom_access_mutex = NULL;
static bool eeprom_answers = false;


bool eeprom_init(void) {
    eeprom_access_mutex = xSemaphoreCreateMutex();
    if (eeprom_access_mutex == NULL) {
        printf("Unable to create EEPROM mutex\n");
        return false;
    }
    cat24c256_eeprom_init();
    eeprom_answers = cat24c256_present();
    if (!eeprom_answers) {
        printf("EEPROM: the chip does not answer: every module runs on its defaults, nothing is stored\n");
    }
    return eeprom_answers;
}


bool eeprom_reachable(void) {
    return eeprom_answers;
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

#if RC_TEST_RECORD_DAMAGED
    // A test build (ADR-017, gate 1): the charge's record read with a byte wrong, every time, as a chip holding damage
    // gives it back; the chip itself is left as it is
    uint16_t damaged = EEPROM_CHARGE_MODE_BASE_ADDR + 4;
    if (is_ok && data_addr <= damaged && damaged < data_addr + len) {
        data[damaged - data_addr] ^= 0x5A;
    }
#endif
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


eeprom_erase_t eeprom_erase(void) {
    if (config_holding_writes()) {
        printf("EEPROM: no factory reset while an image is on trial\n");
        return EEPROM_ERASE_ON_TRIAL;
    }
    if (!eeprom_answers) {
        return EEPROM_ERASE_UNREACHABLE;
    }
    // Held from here to the restart: a save between the last page and the restart would survive the reset
    _take_mutex(xTaskGetSchedulerState());
    bool erased = cat24c256_eeprom_erase();
    if (!erased) {
        printf("EEPROM: the erase failed: the settings may be left in part\n");
    }
    return erased ? EEPROM_ERASED : EEPROM_ERASE_FAILED;
}
