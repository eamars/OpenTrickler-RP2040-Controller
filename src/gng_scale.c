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
        char data[7];
        char unit[3];
        char terminator[2];
    };
    char packet[14];
} gngscale_standard_data_format_t;


// Forward declaration
void _gng_scale_listener_task(void *p);
void scalegng_press_print_key();
void scalegng_press_tare_key();

extern scale_config_t scale_config;

// Instance of the scale handle for G&G JJB series
scale_handle_t gng_scale_handle = {
    .read_loop_task = _gng_scale_listener_task,
    .force_zero = scalegng_press_tare_key,
};


static float _decode_measurement_msg(gngscale_standard_data_format_t * msg) {
    // Decode header
    // Doesn't really matter though..

    // Decode weight information
    float weight = strtof(msg->data, NULL);

    return weight;
}

char REQUEST_DATA_TRANSFER_CMD[4] = {'!','p','\r','\n'};

//read UART
void _gng_scale_listener_task(void *p) {
    char string_buf[20];
    uint8_t string_buf_idx = 0;

    while (true) {
        // Read all data 
            uart_puts(SCALE_UART, REQUEST_DATA_TRANSFER_CMD);

        while (uart_is_readable(SCALE_UART)) {   
            char ch = uart_getc(SCALE_UART);
            
            string_buf[string_buf_idx++] = ch;

            // If we have received 14 bytes then we can decode the message
            if (string_buf_idx == sizeof(gngscale_standard_data_format_t)) {
                // Data is ready, send to decode
                scale_config.current_scale_measurement = _decode_measurement_msg((gngscale_standard_data_format_t *) string_buf);

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

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

//G&G JJB key function
//C4 communication setting - data signal command control 
//standard ESC 0x1B to ! 0x22
//hint:
//For better syntax change standard setting 'C4' 27 'ESC' symbol at scale to 33 '!' sign 

//ESC p -> 0x1b 0x70 0x0D 0x0A standard setting
// ! p -> 0x21 0x70 0x0D 0x0A
void scalegng_press_print_key() {
    char cmd[] = {'!','p','\r','\n'};
    scale_write(cmd, strlen(cmd));
}

//ESC t -> 0x1B 0x74 0x0D 0x0A standard setting
// ! t -> 0x21 0x74 0x0D 0x0A
void scalegng_press_tare_key() {
    char cmd[] = {'!','t','\r','\n'};
    scale_write(cmd, strlen(cmd));
}

//ESC s -> 0x1B 0x73 0x0D 0x0A standard setting
// ! s -> 0x21 0x73 0x0D 0x0A
void scalegng_press_weight_key() {
    char cmd[] = {'!','s','\r','\n'};
    scale_write(cmd, strlen(cmd));
}

//ESC q -> 0x1B 0x71 0x0D 0x0A standard setting
// ! q -> 0x21 0x71 0x0D 0x0A
void scalegng_press_cal_key() {
    char cmd[] = {'!','q','\r','\n'};
    scale_write(cmd, strlen(cmd));
}

// ESC u -> 0x1B 0x75 0x0D 0x0A standard setting
// ! u -> 0x21 0x75 0x0D 0x0A
void scalegng_display_light() {
    char cmd[] = {'!','u','\r','\n'};
    scale_write(cmd, strlen(cmd));
}

// AppState_t scale_enable_fast_report(AppState_t prev_state) {
//     // TODO: Finish this
// }
