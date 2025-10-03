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
-0000.00 GN \r\n
+0000.00 GN \r\n
+0142.02 GN \r\n
+0.32445 oz \r\n
+045.991 ct \r\n
+0.02027 lb \r\n 
+009.198 g  \r\n
*/

typedef union {
    struct __attribute__((__packed__)) {
        char header[1];         // Sign (+ or -)
        char data[7];           // Unsigned integer with leading zeros
        char _space;            // Space
        char unit[2];           // Unit GN (or something else)
        char _space2;           // Space   
        char terminator[2];     // \r\n (carriage return)
    };
    char bytes[14];
} creedmoor_data_format_t ;

// Forward declaration
void _creedmoor_scale_listener_task(void *p);
extern scale_config_t scale_config;
static void force_zero();

// Instance of the scale handle for Creedmoor scale
scale_handle_t creedmoor_scale_handle = {
    .read_loop_task = _creedmoor_scale_listener_task,
    .force_zero = force_zero,
};

static float _decode_measurement_msg(creedmoor_data_format_t * msg) {
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

void _creedmoor_scale_listener_task(void *p) {
    uint8_t string_buf_idx = 0;
    creedmoor_data_format_t frame;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            frame.bytes[string_buf_idx++] = ch;

            // If we have received 14 bytes then we can decode the message
            if (string_buf_idx == sizeof(creedmoor_data_format_t)) {
                // Data is ready, send to decode
                scale_config.current_scale_measurement = _decode_measurement_msg(&frame);

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
    // TODO: Not implemented
}