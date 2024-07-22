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
        char header;
        char _space;
        char stable_state;
        char symbol;
        char weighing_data[9];
        char _space2;
        char weighing_unit[3];
        char terminator[2];
    };
    char bytes[19];
} jm_science_frame_data_format_t;


const static char JM_SCIENCE_FRAME_HEADER = 'E';


// Forward declaration
void _jm_science_scale_listener_task(void *p);
static void force_zero();

extern scale_config_t scale_config;

// Instance of the scale handle for JM Sciense FA series
scale_handle_t jm_science_scale_handle = {
    .read_loop_task = _jm_science_scale_listener_task,
    .force_zero = force_zero,
};


static float _decode_measurement_msg(jm_science_frame_data_format_t * frame) {
    // decode sign 
    int sign = 1;
    if (frame->symbol == '-') {
        sign = -1;
    }

    // Decode weight information
    char *endptr;
    float weight = strtof(frame->weighing_data, &endptr);

    if( endptr == frame->weighing_data ) {
        // Conversion failed
        return nanf(frame->weighing_data);
    }

    weight *= sign;

    return weight;
}


void _jm_science_scale_listener_task(void *p) {
    jm_science_frame_data_format_t frame;
    uint8_t byte_idx = 0;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            // Determine if the frame header is received
            // If a header is received then we should reset the decode sequence
            if (ch == JM_SCIENCE_FRAME_HEADER) {
                byte_idx = 0;
            }

            frame.bytes[byte_idx++] = ch;

            // If we have received 17 bytes then we can decode the message
            if (byte_idx == sizeof(jm_science_frame_data_format_t)) {
                // Data is ready, send to decode
                scale_config.current_scale_measurement = _decode_measurement_msg(&frame);

                // Signal the data is ready
                if (scale_config.scale_measurement_ready) {
                    xSemaphoreGive(scale_config.scale_measurement_ready);
                }

                // Reset buffer index to avoid overflow
                byte_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


static void force_zero() {
    // Unsupported
}

