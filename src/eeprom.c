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
#include "profile.h"


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


uint32_t rnd(void){
    int k, random=0;
    volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    
    for(k=0;k<32;k++){
    
    random = random << 1;
    random = random + (0x00000001 & (*rnd_reg));

    }
    return random;
}


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

    // Register to eeprom save all
    eeprom_register_handler(eeprom_config_save);

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
    // Mappings
    // s0 (str): unique_id
    // s1 (str): version_string
    // s2 (str): vcs_hash
    // s3 (str): build_type
    // s4 (bool): save_to_eeprom
    // s5 (bool): software_reset
    // s6 (bool): erase_eeprom
    static char eeprom_config_json_buffer[256];

    const char * software_reset_string;

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
             "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
             "{\"s0\":\"%s\",\"s1\":\"%s\",\"s2\":\"%s\",\"s3\":\"%s\",\"s4\":%s,\"s5\":%s,\"s6\":%s}", 
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
