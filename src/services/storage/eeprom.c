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
#include "system_control.h"



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


bool eeprom_init(void) {
    bool is_ok = true;
    eeprom_access_mutex = xSemaphoreCreateMutex();

    if (eeprom_access_mutex == NULL) {
        printf("Unable to create EEPROM mutex\n");
        return false;
    }
    
    cat24c256_eeprom_init();

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
