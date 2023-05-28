
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <stdlib.h>

#include "hardware/uart.h"
#include "configuration.h"
#include "scale.h"
#include "eeprom.h"
#include "app.h"


typedef union {
    struct __attribute__((__packed__)){
        char header[2];
        char comma;
        char data[9];
        char unit[3];
        char terminator[2];
    };
    char packet[17];
} scale_standard_data_format_t;


// In this case we will only use eeprom data format to store information
eeprom_scale_data_t scale_data;


// Statics (to be shared between multiple tasks)
static float current_scale_measurement = NAN;
SemaphoreHandle_t scale_measurement_ready;
SemaphoreHandle_t scale_serial_write_access_mutex = NULL;


static inline void _take_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreTake(scale_serial_write_access_mutex, portMAX_DELAY);
    }
}


static inline void _give_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreGive(scale_serial_write_access_mutex);
    }
}


void scale_write(char * command, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();

    _take_mutex(scheduler_state);

    uart_write_blocking(SCALE_UART, (uint8_t *) command, len);

    _give_mutex(scheduler_state);
}


float _decode_measurement_msg(scale_standard_data_format_t * msg) {
    // Decode header
    // Doesn't really matter though..

    // Decode weight information
    float weight = strtof(msg->data, NULL);

    return weight;
}

bool scale_init() {
    bool is_ok;

    uart_init(SCALE_UART, SCALE_UART_BAUDRATE);

    gpio_set_function(SCALE_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(SCALE_UART_RX, GPIO_FUNC_UART);

    // Read config from EEPROM
    is_ok = eeprom_read(EEPROM_SCALE_CONFIG_BASE_ADDR, (uint8_t *) &scale_data, sizeof(eeprom_scale_data_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_SCALE_CONFIG_BASE_ADDR);
        return false;
    }

    // If the revision doesn't match then re-initialize the config
    if (scale_data.scale_data_rev != EEPROM_SCALE_DATA_REV) {
        scale_data.scale_data_rev = EEPROM_SCALE_DATA_REV;
        scale_data.scale_unit = SCALE_UNIT_GRAIN;

        // Write data back
        is_ok = eeprom_write(EEPROM_SCALE_CONFIG_BASE_ADDR, (uint8_t *) &scale_data, sizeof(eeprom_scale_data_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_SCALE_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Create control variables
    // Semaphore to indicate the availability of new measurement. 
    scale_measurement_ready = xSemaphoreCreateBinary();

    // Mutex to control the access to the serial port write
    scale_serial_write_access_mutex = xSemaphoreCreateMutex();

    return is_ok;
}


bool scale_config_save() {
    bool is_ok;
    is_ok = eeprom_write(EEPROM_SCALE_CONFIG_BASE_ADDR, (uint8_t *) &scale_data, sizeof(eeprom_scale_data_t));
    return is_ok;
}


void scale_listener_task(void *p) {
    char string_buf[20];
    uint8_t string_buf_idx = 0;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            string_buf[string_buf_idx++] = ch;

            // If we have received 17 bytes then we can decode the message
            if (string_buf_idx == 17) {
                // Data is ready, send to decode
                current_scale_measurement = _decode_measurement_msg((scale_standard_data_format_t *) string_buf);

                // Signal the data is ready
                if (scale_measurement_ready) {
                    xSemaphoreGive(scale_measurement_ready);
                }

                // Reset
                string_buf_idx = 0;
            }

            // \n is the terminator. We shall reset the receive of message on receiving any of those character.
            if (ch =='\n') {
                string_buf_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


float scale_get_current_measurement() {
    return current_scale_measurement;
}

float scale_block_wait_for_next_measurement() {
    // You can only call this once the scheduler starts
    xSemaphoreTake(scale_measurement_ready, portMAX_DELAY);
    return scale_get_current_measurement();
}


void scale_press_re_zero_key() {
    char cmd[] = "Z\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_press_print_key() {
    char cmd[] = "PRT\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_press_sample_key() {
    char cmd[] = "SMP\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_press_mode_key() {
    char cmd[] = "U\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_press_cal_key() {
    char cmd[] = "CAL\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_press_on_off_key() {
    char cmd[] = "P\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_display_off() {
    char cmd[] = "OFF\r\n";
    scale_write(cmd, strlen(cmd));
}

void scale_display_on() {
    char cmd[] = "ON\r\n";
    scale_write(cmd, strlen(cmd));
}



// AppState_t scale_enable_fast_report(AppState_t prev_state) {
//     // TODO: Finish this
// }



bool http_rest_scale_weight(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char scale_weight_to_json_buffer[32];

    sprintf(scale_weight_to_json_buffer, 
            "{\"weight\":%f}", 
            scale_get_current_measurement());

    size_t data_length = strlen(scale_weight_to_json_buffer);
    file->data = scale_weight_to_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
