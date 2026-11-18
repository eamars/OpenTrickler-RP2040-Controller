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
#include "unit_id.h"


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
eeprom_metadata_t metadata;
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

    // Generate a default metadata (in case the metadata is invalid)
    eeprom_metadata_t default_eeprom_metadata;

    // The id, made from the chip's own (ADR-008, gate 4): it was a random number, drawn again at a factory reset
    char id[UNIT_ID_LENGTH + 1];
    unit_id_get(id, sizeof(id));
    memcpy(default_eeprom_metadata.unique_id, id, sizeof(default_eeprom_metadata.unique_id));

    // Generate rev
    default_eeprom_metadata.eeprom_metadata_rev = 0x0;

    // Load eeprom data
    memset(&metadata, 0x0, sizeof(metadata));
    is_ok = load_config(EEPROM_METADATA_BASE_ADDR, &metadata, &default_eeprom_metadata, sizeof(metadata), EEPROM_METADATA_REV);
    if (!is_ok) {
        printf("Unable to read EEPROM configuration\n");
    }
    // A unit whose stored id was drawn at random takes the chip's, once, and stores it: the EEPROM says what it is
    if (memcmp(metadata.unique_id, id, sizeof(metadata.unique_id)) != 0) {
        memcpy(metadata.unique_id, id, sizeof(metadata.unique_id));
        eeprom_config_save();
    }

    // Register to eeprom save all
    eeprom_register_handler(eeprom_config_save);

    return is_ok;
}

bool eeprom_config_save() {
    bool is_ok = save_config(EEPROM_METADATA_BASE_ADDR, &metadata, sizeof(metadata));
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


bool eeprom_get_board_id(char *board_id_buffer, size_t bytes_to_copy) {
    memcpy(board_id_buffer, metadata.unique_id, bytes_to_copy);

    return true;
}