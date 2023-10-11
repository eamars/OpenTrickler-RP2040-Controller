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

extern scale_handle_t and_fxi_scale_handle;
extern scale_handle_t steinberg_scale_handle;
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
    set_scale_driver(scale_config.persistent_config.scale_driver);

    // Create the Task for the listener loop
    xTaskCreate(scale_config.scale_handle->read_loop_task, "Scale Task", configMINIMAL_STACK_SIZE, NULL, 9, NULL);

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


void scale_write(char * command, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();

    _take_mutex(scheduler_state);

    uart_write_blocking(SCALE_UART, (uint8_t *) command, len);

    _give_mutex(scheduler_state);
}


bool http_rest_scale_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char scale_config_to_json_buffer[256];

    // Set value
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "driver") == 0) {
            if (strcmp(values[idx], "AND FX-i Std") == 0) {
                set_scale_driver(SCALE_DRIVER_AND_FXI);
            }
            else if (strcmp(values[idx], "Steinberg SBS") == 0) {
                set_scale_driver(SCALE_DRIVER_STEINBERG_SBS);
            }
        }
        else if (strcmp(params[idx], "baudrate") == 0) {
            if (strcmp(values[idx], "4800") == 0) {
                scale_config.persistent_config.scale_baudrate = BAUDRATE_4800;
            }
            else if (strcmp(values[idx], "9600") == 0) {
                scale_config.persistent_config.scale_baudrate = BAUDRATE_9600;
            }
            else if (strcmp(values[idx], "19200") == 0) {
                scale_config.persistent_config.scale_baudrate = BAUDRATE_19200;
            }
        }
    }

    // Convert config to string
    const char * scale_driver_string = get_scale_driver_string();

    snprintf(scale_config_to_json_buffer, 
             sizeof(scale_config_to_json_buffer),
             "{\"driver\":\"%s\",\"baudrate\":%"PRId32"}", 
             scale_driver_string, 
             get_scale_baudrate(scale_config.persistent_config.scale_baudrate));
    
    size_t data_length = strlen(scale_config_to_json_buffer);
    file->data = scale_config_to_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_scale_weight(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char scale_weight_to_json_buffer[32];

    snprintf(scale_weight_to_json_buffer, 
             sizeof(scale_weight_to_json_buffer),
             "{\"weight\":%0.3f}", 
             scale_get_current_measurement());

    size_t data_length = strlen(scale_weight_to_json_buffer);
    file->data = scale_weight_to_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


float scale_get_current_measurement() {
    return scale_config.current_scale_measurement;
}


float scale_block_wait_for_next_measurement() {
    // You can only call this once the scheduler starts
    xSemaphoreTake(scale_config.scale_measurement_ready, portMAX_DELAY);
    return scale_get_current_measurement();
}

