#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eeprom.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "common.h"
#include "wireless.h"
#include "app.h"
#include "neopixel_led.h"
#include "mini_12864_module.h"
#include "profile.h"
#include "system_control.h"


extern bool cat24c256_eeprom_erase();
extern void cat24c256_eeprom_init();
extern bool cat24c256_write(uint16_t data_addr, uint8_t * data, size_t len);
extern bool cat24c256_read(uint16_t data_addr, uint8_t * data, size_t len);

// Linked list implementation
typedef struct _eeprom_save_handler_node {
    eeprom_save_handler_t function_handler;
    struct _eeprom_save_handler_node * next;
} _eeprom_save_handler_node_t;

// Singleton variables
SemaphoreHandle_t eeprom_access_mutex = NULL;
static _eeprom_save_handler_node_t * eeprom_save_handler_head = NULL;


void eeprom_register_handler(eeprom_save_handler_t handler) {
    _eeprom_save_handler_node_t * new_node = malloc(sizeof(_eeprom_save_handler_node_t));
    new_node->function_handler = handler;

    // Append to the head
    new_node->next = eeprom_save_handler_head;
    eeprom_save_handler_head = new_node;
}


uint8_t eeprom_save_all() {
    // Iterate over all registered handlers and run the save functions
    for (_eeprom_save_handler_node_t * node = eeprom_save_handler_head; node != NULL; node = node->next) {
        // Run the save handler
        node->function_handler();
    }
    return 37;  // Configuration Menu ID
}


uint8_t eeprom_erase(bool reboot) {
    cat24c256_eeprom_erase();

    if (reboot) {
        software_reboot();
    }
    
    return 37;  // Configuration Menu ID
}


/*
 * The record at 0K held the unit's id, drawn at random, until ADR-008's gate 4 read the id from the Pico. Cleared once,
 * at the first start that is not a trial's, so the EEPROM keeps no id the unit no longer has, and a module given the
 * slot later finds it blank (a record whose first two bytes are 0 would pass load_config's old revision check). Left
 * on trial: an image that goes back reads its id there
 */
static void eeprom_clear_old_id(void) {
    uint8_t page[EEPROM_OLD_ID_BYTES];
    if (!cat24c256_read(EEPROM_METADATA_BASE_ADDR, page, sizeof(page))) {
        return;
    }
    bool blank = true;
    for (size_t idx = 0; idx < sizeof(page); idx++) {
        blank = blank && page[idx] == 0xff;
    }
    if (blank) {
        return;
    }
    // The old record: a 16-bit revision, then the id's 8 characters with no ending
    if (config_writes_held()) {
        printf("EEPROM: on trial, the old id's record (%.8s) left until the update is kept\n", (const char *) &page[2]);
        return;
    }
    printf("EEPROM: the old id's record cleared (it held %.8s)\n", (const char *) &page[2]);
    memset(page, 0xff, sizeof(page));
    cat24c256_write(EEPROM_METADATA_BASE_ADDR, page, sizeof(page));
}


bool eeprom_init(void) {
    bool is_ok = true;
    eeprom_access_mutex = xSemaphoreCreateMutex();

    if (eeprom_access_mutex == NULL) {
        printf("Unable to create EEPROM mutex\n");
        return false;
    }
    
    cat24c256_eeprom_init();
    eeprom_clear_old_id();

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
