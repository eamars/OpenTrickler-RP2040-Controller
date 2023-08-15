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


// Forward declaration
void scale_listener_task(void *p);
void scale_write(char * command, size_t len);
extern scale_config_t scale_config;

// Instance of the scale handle for A&D FXi series
scale_handle_t and_fxi_scale_handle = {
    .read_loop_task = scale_listener_task,
    .write = scale_write
};



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


float _decode_measurement_msg(scale_standard_data_format_t * msg) {
    // Decode header
    // Doesn't really matter though..

    // Decode weight information
    float weight = strtof(msg->data, NULL);

    return weight;
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
                scale_config.current_scale_measurement = _decode_measurement_msg((scale_standard_data_format_t *) string_buf);

                // Signal the data is ready
                if (scale_config.scale_measurement_ready) {
                    xSemaphoreGive(scale_config.scale_measurement_ready);
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
