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


// Forward declaration
void _steinberg_scale_listener_task(void *p);
extern scale_config_t scale_config;

// Instance of the scale handle for A&D FXi series
scale_handle_t steinberg_scale_handle = {
    .read_loop_task = _steinberg_scale_listener_task,
};


void _steinberg_scale_listener_task(void *p) {
    char string_buf[20];
    uint8_t string_buf_idx = 0;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);
            /////////////////////////////////////////////////////////////////////
            // TODO: I'm not sure how to decode, please provide example here...//
            /////////////////////////////////////////////////////////////////////


            // string_buf[string_buf_idx++] = ch;

            // // If we have received 17 bytes then we can decode the message
            // if (string_buf_idx == 17) {
            //     // Data is ready, send to decode
            //     scale_config.current_scale_measurement = _decode_measurement_msg((scale_standard_data_format_t *) string_buf);

            //     // Signal the data is ready
            //     if (scale_config.scale_measurement_ready) {
            //         xSemaphoreGive(scale_config.scale_measurement_ready);
            //     }

            //     // Reset
            //     string_buf_idx = 0;
            // }

            // // \n is the terminator. We shall reset the receive of message on receiving any of those character.
            // if (ch =='\n') {
            //     string_buf_idx = 0;
            // }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}