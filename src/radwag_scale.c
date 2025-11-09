#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/uart.h"
#include "configuration.h"
#include "scale.h"
#include "app.h"

// Radwag response frame structure for SUI command
// Format: SUI<stability><mass(12)><unit(3)>CR LF
// Example: "SUI        1.56 gr \r\n" (stable)
//          "SUI?       2.18 gr \r\n" (unstable)
typedef union {
    struct __attribute__((__packed__)) {
        char command[3];        // "SUI" - immediate reading in current unit
        char stability;         // ' ' stable, '?' unstable, '^' overflow+, 'v' overflow-
        char mass[12];          // 12 chars right-aligned with spaces and dot
        char unit[3];           // 3 chars (usually "gr " with trailing space)
        char terminator[2];     // CR LF
    };
    char bytes[21];
} radwag_sui_frame_t;

// Forward declarations
void _radwag_scale_listener_task(void *p);
void radwag_scale_press_re_zero_key();

extern scale_config_t scale_config;

// Instance of the scale handle for Radwag PS R2 series
scale_handle_t radwag_ps_r2_scale_handle = {
    .read_loop_task = _radwag_scale_listener_task,
    .force_zero = radwag_scale_press_re_zero_key,
};

/**
 * @brief Decode mass measurement message from Radwag scale
 * 
 * @param msg Pointer to mass frame structure
 * @return float Decoded weight value, NaN if conversion failed
 */
static float _decode_measurement_msg(radwag_sui_frame_t *msg) {
    // Create a temporary buffer for mass conversion
    // Mass field is 12 characters with spaces padding on the left
    char mass_str[13];
    memcpy(mass_str, msg->mass, 12);
    mass_str[12] = '\0';
    
    // Convert mass string to float (strtof handles leading spaces)
    char *endptr;
    float weight = strtof(mass_str, &endptr);
    
    if (endptr == mass_str) {
        // Conversion failed
        return nanf(mass_str);
    }
    
    // Note: The mass field in SUI doesn't have a separate sign character
    // Negative values would include '-' in the mass field itself
    
    return weight;
}

/**
 * @brief Main listener task for Radwag scale communication
 * Continuously reads data from continuous transmission mode
 * 
 * @param p Task parameter (unused)
 */
void _radwag_scale_listener_task(void *p) {
    uint8_t string_buf_idx = 0;
    radwag_sui_frame_t frame;
    
    while (true) {
        // Read all available data
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);
            frame.bytes[string_buf_idx++] = ch;
            
            // Radwag SUI frame is 21 bytes
            if (string_buf_idx == sizeof(radwag_sui_frame_t)) {
                // Verify this is a SUI mass frame
                if (frame.command[0] == 'S' && 
                    frame.command[1] == 'U' && 
                    frame.command[2] == 'I') {
                    
                    // Optional: Check stability flag
                    // ' ' = stable, '?' = unstable
                    // You can decide whether to accept unstable readings
                    bool is_stable = (frame.stability == ' ');
                    
                    // Data is ready, decode and update
                    scale_config.current_scale_measurement = _decode_measurement_msg(&frame);
                    
                    // Signal that data is ready
                    if (scale_config.scale_measurement_ready) {
                        xSemaphoreGive(scale_config.scale_measurement_ready);
                    }
                }
                
                // Reset buffer
                string_buf_idx = 0;
            }
            
            // Reset on line terminator to resynchronize if out of sync
            if (ch == '\n') {
                string_buf_idx = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief Zero the scale (send Z command)
 * Command: Z\r\n
 */
void radwag_scale_press_re_zero_key() {
    char cmd[] = "T\r\n";  // Tare command (not Zero)
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Tare the scale (send T command)
 * Command: T\r\n
 */
void radwag_scale_press_tare_key() {
    char cmd[] = "T\r\n";
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Enable continuous transmission in current unit
 * Command: CU1\r\n
 * Call this once during initialization if not already enabled on scale
 */
void radwag_scale_enable_continuous_transmission() {
    char cmd[] = "CU1\r\n";
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Disable continuous transmission in current unit
 * Command: CU0\r\n
 */
void radwag_scale_disable_continuous_transmission() {
    char cmd[] = "CU0\r\n";
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Set tare value
 * Command: UT <value>\r\n
 * 
 * @param tare_value Tare value to set (use dot as decimal separator)
 */
void radwag_scale_set_tare(float tare_value) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "UT %.3f\r\n", tare_value);
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Lock scale keyboard
 * Command: K1\r\n
 */
void radwag_scale_lock_keyboard() {
    char cmd[] = "K1\r\n";
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Unlock scale keyboard
 * Command: K0\r\n
 */
void radwag_scale_unlock_keyboard() {
    char cmd[] = "K0\r\n";
    scale_write(cmd, strlen(cmd));
}

/**
 * @brief Activate beep signal
 * Command: BP <time_ms>\r\n
 * 
 * @param duration_ms Duration in milliseconds (recommended 50-5000)
 */
void radwag_scale_beep(uint16_t duration_ms) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "BP %u\r\n", duration_ms);
    scale_write(cmd, strlen(cmd));
}
