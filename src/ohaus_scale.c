#include <ctype.h>
#include <stdlib.h>

#include "ohaus_scale.h"
#include "scale.h"

#include "FreeRTOS.h"
#include "task.h"
#include "configuration.h"


void _ohaus_scale_listener_task(void *p);
void _ohaus_scale_force_zero(void);


scale_handle_t ohaus_scale_handle = {
    .read_loop_task = _ohaus_scale_listener_task,
    .force_zero = _ohaus_scale_force_zero,
};

extern scale_config_t scale_config;


/**
 * @brief Send tare/re-zero command to Ohaus Pioneer PR/PX scale
 *
 * Ohaus Pioneer command: "T\r\n"
 */
void _ohaus_scale_force_zero(void) {
    scale_write("T\r\n", 3);
}


/**
 * @brief Ohaus Pioneer PR/PX scale listener task
 *
 * Ohaus Pioneer outputs continuous ASCII lines of the form:
 *   "ST,GS, +      0.0000 g \r\n"  (stable gross)
 *   "US,GS, -      1.2345 g \r\n"  (unstable)
 *
 * The sign and digits may be separated by spaces, so a standard strtof()
 * from the first sign/digit is not sufficient. This parser explicitly
 * handles the sign-space-digits pattern used by Ohaus scales.
 */
void _ohaus_scale_listener_task(void *p) {
    char rx_buffer[64];
    uint8_t rx_buffer_idx = 0;

    while (true) {
        // Read all available data
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            // Prevent buffer overflow
            if (rx_buffer_idx >= sizeof(rx_buffer) - 1) {
                rx_buffer_idx = 0;
            }

            rx_buffer[rx_buffer_idx++] = ch;

            // Parse on newline
            if (ch == '\n') {
                rx_buffer[rx_buffer_idx] = '\0';

                // Scan for sign character (+ or -) which precedes the value
                for (char *p = rx_buffer; *p != '\0'; p++) {
                    if (*p == '+' || *p == '-') {
                        float sign = (*p == '-') ? -1.0f : 1.0f;

                        // Skip spaces between sign and digits
                        char *q = p + 1;
                        while (*q == ' ') {
                            q++;
                        }

                        // Attempt float conversion from first digit
                        if (*q != '\0' && (isdigit((unsigned char)*q) || *q == '.')) {
                            char *endptr;
                            float weight = strtof(q, &endptr);

                            if (endptr != q) {
                                scale_config.current_scale_measurement = sign * weight;
                                if (scale_config.scale_measurement_ready) {
                                    xSemaphoreGive(scale_config.scale_measurement_ready);
                                }
                                break;
                            }
                        }
                    }
                }

                rx_buffer_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
