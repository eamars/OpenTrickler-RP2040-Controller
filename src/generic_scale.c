#include "generic_scale.h"
#include "scale.h"

#include "FreeRTOS.h"
#include "task.h"
#include "configuration.h"



void _generic_scale_listener_task(void *p);
void _generic_scale_init(void *self);


scale_handle_t generic_scale_drv_handle = {
    .read_loop_task = _generic_scale_listener_task,
    .force_zero = NULL,
};
extern scale_config_t scale_config;


/**
 * @brief Generic scale listener task
 */
void _generic_scale_listener_task(void *p) {
    char rx_buffer[32];
    uint8_t rx_buffer_idx = 0;

    while (true) {
        // Read all available data
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            // Prevent buffer overflow
            if (rx_buffer_idx >= sizeof(rx_buffer) - 1) {
                // Reset buffer index
                rx_buffer_idx = 0;
            }

            // Accept the buffer
            rx_buffer[rx_buffer_idx++] = ch;

            // Stop condition 1: When \n is received
            if (ch == '\n') {
                // Null terminate the string
                rx_buffer[rx_buffer_idx] = '\0';

                // Parse the received string to float
                char *startptr = rx_buffer;
                char *endptr;

                // 1. look for the start of a number: a digit, a decimal point, or a sign
                //     * Noted this will skip the weight prefix like "ST" etc.
                //     * Noted the string is already null terminated
                while (*startptr && 
                    !isdigit((unsigned char)*startptr) && 
                    *startptr != '-' && *startptr != '+') {
                    startptr++;
                }

                // 2. Attempt to convert from the first numeric-looking character. 
                float weight = strtof(startptr, &endptr);

                // If the conversion is successful then post the measurement.
                if (endptr != startptr) {
                    scale_publish_measurement(weight);
                }

                // Reset buffer index
                rx_buffer_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}