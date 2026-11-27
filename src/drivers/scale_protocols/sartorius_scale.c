#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "chip_serial.h"
#include "configuration.h"
#include "scale_make.h"

// Sartorius typically sends data in format like: "+  123.456 g" or similar
// We'll buffer until we get a complete line (ends with \r or \n)
typedef struct {
    char buffer[32];
    uint8_t index;
} sartorius_buffer_t;

// Forward declaration
void _sartorius_scale_listener_task(void *p);
static void force_zero();

// Instance of the scale handle for Sartorius series
scale_handle_t sartorius_scale_handle = {
    .read_loop_task = _sartorius_scale_listener_task,
    .force_zero = force_zero,
};

static float _decode_measurement_msg(const char *msg, size_t len) {
    // Parse Sartorius format: 
    // Examples: "     0.000 GN" or "+   27.350" or "+   62.916 GN"
    // Format: optional sign, spaces, decimal number, optional spaces and unit
    
    int sign = 1;
    size_t start = 0;
    
    // Check for sign at the beginning
    if (len > 0 && (msg[0] == '-' || msg[0] == '+')) {
        if (msg[0] == '-') {
            sign = -1;
        }
        start = 1;
    }
    
    // Skip leading spaces
    while (start < len && msg[start] == ' ') {
        start++;
    }
    
    // Parse the number using strtof which will stop at the first non-numeric character. A frame with no number in it
    // is NaN, as the other drivers say it, not 0.0, which would read as an empty pan (ADR-013)
    if (start >= len) {
        return nanf("");
    }
    char *endptr;
    float value = strtof(&msg[start], &endptr);
    if (endptr == &msg[start]) {
        return nanf("");
    }

    return sign * value;
}

void _sartorius_scale_listener_task(void *p) {
    sartorius_buffer_t buf = {0};
    
    while (true) {
        // Read all available data
        int got;
        while ((got = chip_serial_read(SCALE_UART)) != CHIP_SERIAL_NONE) {
            char ch = (char) got;
            
            // Look for line terminators
            if (ch == '\r' || ch == '\n') {
                if (buf.index > 0) {
                    // We have a complete message
                    buf.buffer[buf.index] = '\0';
                    
                    // Decode the measurement
                    float weight = _decode_measurement_msg(buf.buffer, buf.index);
                    
                    // Publish it; a frame with no number is dropped there
                    scale_make_host->publish_measurement(weight);
                    
                    // Reset buffer
                    buf.index = 0;
                    memset(buf.buffer, 0, sizeof(buf.buffer));
                }
            } else if (buf.index < sizeof(buf.buffer) - 1) {
                // Add character to buffer
                buf.buffer[buf.index++] = ch;
            } else {
                // Buffer overflow, reset
                buf.index = 0;
                memset(buf.buffer, 0, sizeof(buf.buffer));
            }
        }
        
        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void force_zero() {
    // TODO: Send force zero command to Sartorius scale
    // Typically this might be a command like "Z\r" or "0\r"
    // depending on the specific scale model
}
