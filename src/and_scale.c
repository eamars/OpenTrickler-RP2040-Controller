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
void _and_scale_listener_task(void *p);
extern scale_config_t scale_config;

// Instance of the scale handle for A&D FXi series
scale_handle_t and_fxi_scale_handle = {
    .read_loop_task = _and_scale_listener_task,
};


float _decode_measurement_msg(scale_standard_data_format_t * msg) {
    // Decode header
    // Doesn't really matter though..

    // Decode weight information
    float weight = strtof(msg->data, NULL);

    return weight;
}


void _and_scale_listener_task(void *p) {
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
