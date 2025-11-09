#include <math.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <inttypes.h>

#include "configuration.h"
#include "scale.h"
#include "eeprom.h"
#include "app.h"
#include "scale.h"
#include "common.h"

extern scale_handle_t and_fxi_scale_handle;
extern scale_handle_t steinberg_scale_handle;
extern scale_handle_t ussolid_scale_handle;
extern scale_handle_t gng_scale_handle;
extern scale_handle_t jm_science_scale_handle;
extern scale_handle_t creedmoor_scale_handle;
extern scale_handle_t radwag_ps_r2_scale_handle;

scale_config_t scale_config;



void set_scale_driver(scale_driver_t scale_driver) {
    // Update the persistent settings
    scale_config.persistent_config.scale_driver = scale_driver;
    
    switch (scale_driver) {
        case SCALE_DRIVER_AND_FXI:
        {
            scale_config.scale_handle = &and_fxi_scale_handle;
            break;
        }
        case SCALE_DRIVER_STEINBERG_SBS:
        {
            scale_config.scale_handle = &steinberg_scale_handle;
            break;
        }
        case SCALE_DRIVER_GNG_JJB:
        {
            scale_config.scale_handle = &gng_scale_handle;
            break;
        }
        case SCALE_DRIVER_USSOLID_JFDBS:
        {
            scale_config.scale_handle = &ussolid_scale_handle;
            break;
        }
        case SCALE_DRIVER_JM_SCIENCE:
        {
            scale_config.scale_handle = &jm_science_scale_handle;
            break;
        }
        case SCALE_DRIVER_CREEDMOOR:
        {
            scale_config.scale_handle = &creedmoor_scale_handle;
            break;
        }
        case SCALE_DRIVER_RADWAG_PS_R2:
        {
            scale_config.scale_handle = &radwag_ps_r2_scale_handle;
            break;
        }
        default:
            assert(false);
            break;
    }
}

uint32_t get_scale_baudrate(scale_baudrate_t scale_baudrate) {
    uint32_t baudrate_uint = 0;

    switch (scale_baudrate) {
        case BAUDRATE_4800:
            baudrate_uint = 4800;
            break;
        case BAUDRATE_9600:
            baudrate_uint = 9600;
            break;
        case BAUDRATE_19200:
            baudrate_uint = 19200;
            break;
        default:
            break;
    }

    return baudrate_uint;
}


const char * get_scale_driver_string() {
    const char * scale_driver_string = NULL;

    switch (scale_config.persistent_config.scale_driver) {
        case SCALE_DRIVER_AND_FXI:
            scale_driver_string = "AND FX-i Std";
            break;
        case SCALE_DRIVER_STEINBERG_SBS:
            scale_driver_string = "Steinberg SBS";
            break;
        case SCALE_DRIVER_USSOLID_JFDBS:
            scale_driver_string = "US Solid JFDBS";
            break;
        case SCALE_DRIVER_GNG_JJB:
            scale_driver_string = "GNG JJB";
            break;
        case SCALE_DRIVER_JM_SCIENCE:
            scale_driver_string = "JM Science";
            break;
        case SCALE_DRIVER_CREEDMOOR:
            scale_driver_string = "Creedmoor";
            break;
        case SCALE_DRIVER_RADWAG_PS_R2:
            scale_driver_string = "Radwag PS R2";
            break;
        default:
            break;
    }

    return scale_driver_string;
}


bool scale_init() {
    bool is_ok;

    // Read config from EEPROM
    is_ok = eeprom_read(EEPROM_SCALE_CONFIG_BASE_ADDR, (uint8_t *) &scale_config.persistent_config, sizeof(eeprom_scale_data_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_SCALE_CONFIG_BASE_ADDR);
        return false;
    }

    // If the revision doesn't match then re-initialize the config
    if (scale_config.persistent_config.scale_data_rev != EEPROM_SCALE_DATA_REV) {

        scale_config.persistent_config.scale_data_rev = EEPROM_SCALE_DATA_REV;
        scale_config.persistent_config.scale_driver = SCALE_DRIVER_AND_FXI;
        scale_config.persistent_config.scale_baudrate = BAUDRATE_19200;

        // Write data back
        is_ok = scale_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_SCALE_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Initialize UART
    uart_init(SCALE_UART, get_scale_baudrate(scale_config.persistent_config.scale_baudrate));

    gpio_set_function(SCALE_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(SCALE_UART_RX, GPIO_FUNC_UART);

    // Create control variables
    // Semaphore to indicate the availability of new measurement. 
    scale_config.scale_measurement_ready = xSemaphoreCreateBinary();

    // Mutex to control the access to the serial port write
    scale_config.scale_serial_write_access_mutex = xSemaphoreCreateMutex();

    // Initialize the measurement variable
    scale_config.current_scale_measurement = NAN;

    // Initialize the driver handle
    printf("Scale driver: %x\n", scale_config.persistent_config.scale_driver);
    set_scale_driver(scale_config.persistent_config.scale_driver);

    // Create the Task for the listener loop
    xTaskCreate(scale_config.scale_handle->read_loop_task, "Scale Task", configMINIMAL_STACK_SIZE, NULL, 9, NULL);

    // Register to eeprom save all
    eeprom_register_handler(scale_config_save);

    return is_ok;
}


bool scale_config_save() {
    bool is_ok = eeprom_write(EEPROM_SCALE_CONFIG_BASE_ADDR, (uint8_t *) &scale_config.persistent_config, sizeof(eeprom_scale_data_t));
    return is_ok;
}


static inline void _take_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreTake(scale_config.scale_serial_write_access_mutex, portMAX_DELAY);
    }
}


static inline void _give_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreGive(scale_config.scale_serial_write_access_mutex);
    }
}


void scale_write(const char * command, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();

    _take_mutex(scheduler_state);

    uart_write_blocking(SCALE_UART, (uint8_t *) command, len);

    _give_mutex(scheduler_state);
}


float scale_get_current_measurement() {
    return scale_config.current_scale_measurement;
}


/*
    Block wait for the next available measurement.

    block_time_ms set to 0 to wait indefinitely.
*/
bool scale_block_wait_for_next_measurement(uint32_t block_time_ms, float * current_measurement) {
    TickType_t delay_ticks;

    if (block_time_ms == 0) {
        delay_ticks = portMAX_DELAY;
    }
    else {
        delay_ticks = pdMS_TO_TICKS(block_time_ms);
    }

    // You can only call this once the scheduler starts
    if (xSemaphoreTake(scale_config.scale_measurement_ready, delay_ticks) == pdTRUE){
        *current_measurement = scale_get_current_measurement();

        return true;
    }
    
    // No valid measurement
    return false;
}


bool http_rest_scale_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // s0 (int): driver index
    // s1 (int): baud rate index
    // ee (bool): save to eeprom

    static char scale_config_to_json_buffer[256];
    bool save_to_eeprom = false;

    // Set value
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "s0") == 0) {
            scale_driver_t driver_idx = (scale_driver_t) atoi(values[idx]);
            set_scale_driver(driver_idx);
        }
        else if (strcmp(params[idx], "s1") == 0) {
            scale_baudrate_t baudrate_idx = (scale_baudrate_t) atoi(values[idx]);
            scale_config.persistent_config.scale_baudrate = baudrate_idx;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        scale_config_save();
    }

    snprintf(scale_config_to_json_buffer, 
             sizeof(scale_config_to_json_buffer),
             "%s"
             "{\"s0\":%d,\"s1\":%d}", 
             http_json_header,
             scale_config.persistent_config.scale_driver, 
             scale_config.persistent_config.scale_baudrate);
    
    size_t data_length = strlen(scale_config_to_json_buffer);
    file->data = scale_config_to_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_scale_action(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // a0 (scale_action_t): Command to the scale
    
    // Control
    scale_action_t action = SCALE_ACTION_NO_ACTION;

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "a0") == 0) {
            action = (scale_action_t) atoi(values[idx]);
            
            switch (action) {
                case SCALE_ACTION_FORCE_ZERO:
                    scale_config.scale_handle->force_zero();
                    break;
                default: 
                    break;
            }
        }
    }

    static char json_buffer[64];

    // Response
    snprintf(json_buffer, 
             sizeof(json_buffer),
             "%s"
             "{\"a0\":%d}",
             http_json_header,
             (int) action);

    size_t data_length = strlen(json_buffer);
    file->data = json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}