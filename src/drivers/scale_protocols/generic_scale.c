#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "generic_scale.h"
#include "scale_make.h"

#include "FreeRTOS.h"
#include "task.h"
#include "configuration.h"
#include "chip_serial.h"



void _generic_scale_listener_task(void *p);
void _generic_scale_init(void *self);


/*
    The Generic driver knows no zero command for its scale: asked to zero, it does nothing. It had no function at
    all, and its three callers call it unchecked: a zero press jumped to address 0 (ADR-012; audit S1).
*/
static void _generic_scale_force_zero(void) {
}


scale_handle_t generic_scale_drv_handle = {
    .read_loop_task = _generic_scale_listener_task,
    .force_zero = _generic_scale_force_zero,
};


/*
    A frame's weight: its first number, past any prefix ("ST", "GS,"), from a digit or a point before one, with the
    sign before it, spaces between allowed ("- 12.34", "+ 0.50"). False when the frame holds no number
*/
static bool _generic_scale_parse(const char *frame, float *weight) {
    const char *sign = NULL;
    for (const char *at = frame; *at != '\0'; at += 1) {
        if (*at == '-' || *at == '+') {
            sign = at;
            continue;
        }
        if (*at == ' ' && sign != NULL) {
            continue;
        }
        if (isdigit((unsigned char) at[0]) || (at[0] == '.' && isdigit((unsigned char) at[1]))) {
            float value = strtof(at, NULL);
            *weight = sign != NULL && *sign == '-' ? -value : value;
            return true;
        }
        sign = NULL;
    }
    return false;
}


/**
 * @brief Generic scale listener task
 */
void _generic_scale_listener_task(void *p) {
    char rx_buffer[32];
    uint8_t rx_buffer_idx = 0;

    while (true) {
        // Read all available data
        int got;
        while ((got = chip_serial_read(SCALE_UART)) != CHIP_SERIAL_NONE) {
            char ch = (char) got;

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

                float weight;
                if (_generic_scale_parse(rx_buffer, &weight)) {
                    scale_make_host->publish_measurement(weight);
                }

                // Reset buffer index
                rx_buffer_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}