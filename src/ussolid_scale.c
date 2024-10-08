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

/* 
  Example data
    +   20.758g  
    +   20.758g  
    +   20.758g  
    +  320.344GN 
    +  320.344GN 
    +  320.344GN 
    +  320.344GN 
    +  889.629GN 
    + 1116.438GN 
    + 1320.253GN 
    + 1508.019GN 
    + ~~~~~~~~GN 
*/
typedef union {
    struct __attribute__((__packed__)) {
        char header[2];         // + or -
        char data[8];           // Float integer
        char unit[3];           // GN (or something else)   
        char terminator[2];     // \r\n (carriage return and newline)
    };
    char bytes[15];
} ussolid_jfdbs_data_format_t ;

// Forward declaration
void _ussolid_scale_listener_task(void *p);
extern scale_config_t scale_config;
static void force_zero();

// Instance of the scale handle for US Solid series
scale_handle_t ussolid_scale_handle = {
    .read_loop_task = _ussolid_scale_listener_task,
    .force_zero = force_zero,
};

static float _decode_measurement_msg(ussolid_jfdbs_data_format_t * msg) {
    // Determine sign
    int sign = 1;

    // Check if the header contains a negative sign
    if (msg->header[0] == '-') {
        sign = -1;
    }

    char *endptr;
    float weight = strtof(msg->data, &endptr);

    if( endptr == msg->data ) {
        // Conversion failed
        return nanf(msg->data);
    }

    // Apply the sign
    weight *= sign;

    return weight;
}

void _ussolid_scale_listener_task(void *p) {
    uint8_t string_buf_idx = 0;
    ussolid_jfdbs_data_format_t frame;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            frame.bytes[string_buf_idx++] = ch;

            // If we have received 15 bytes then we can decode the message
            if (string_buf_idx == sizeof(ussolid_jfdbs_data_format_t)) {
                
                // Data is ready, send to decode
                float weight = _decode_measurement_msg(&frame);

                scale_config.current_scale_measurement = weight;

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

static void force_zero() {
    // Unsupported
}

