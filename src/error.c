#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>

#include "error.h"
#include "display.h"
#include "neopixel_led.h"

// Error system state
static error_system_state_t error_state = {
    .eeprom_ready = false,
    .neopixel_ready = false,
    .display_ready = false,
    .wifi_ready = false,
};

// Error log circular buffer
static error_code_t error_log[ERROR_LOG_SIZE];
static uint8_t error_log_count = 0;
static uint8_t error_log_write_idx = 0;

// Last error that occurred
static error_code_t last_error = ERR_NONE;
static bool error_occurred = false;

// Error LED color (red)
#define ERROR_LED_COLOR 0xFF0000

// Display layout constants
#define ERROR_TITLE_HEIGHT 13
#define ERROR_LINE_HEIGHT 8
#define ERROR_MAX_DISPLAY_LINES 5

void error_system_init(void) {
    error_state.eeprom_ready = false;
    error_state.neopixel_ready = false;
    error_state.display_ready = false;
    error_state.wifi_ready = false;
    last_error = ERR_NONE;
    error_occurred = false;
    error_log_count = 0;
    error_log_write_idx = 0;
    memset(error_log, 0, sizeof(error_log));
}

void error_set_eeprom_ready(bool ready) {
    error_state.eeprom_ready = ready;
}

void error_set_neopixel_ready(bool ready) {
    error_state.neopixel_ready = ready;
}

void error_set_display_ready(bool ready) {
    error_state.display_ready = ready;
}

void error_set_wifi_ready(bool ready) {
    error_state.wifi_ready = ready;
}

// Get short category string (max 3 chars)
static const char* error_get_short_category(error_code_t code) {
    if (code >= 100 && code < 200) return "EEP";
    if (code >= 200 && code < 300) return "DSP";
    if (code >= 300 && code < 400) return "LED";
    if (code >= 400 && code < 500) return "WIF";
    if (code >= 500 && code < 600) return "MOT";
    if (code >= 600 && code < 700) return "SCL";
    if (code >= 700 && code < 800) return "SRV";
    if (code >= 800 && code < 900) return "PRF";
    if (code >= 900 && code < 1000) return "CHG";
    if (code >= 1000 && code < 1100) return "RST";
    if (code >= 1100 && code < 1200) return "MEM";
    if (code >= 1200 && code < 1300) return "CAL";
    return "UNK";
}

const char* error_code_to_string(error_code_t code) {
    switch (code) {
        case ERR_NONE: return "No error";

        // EEPROM
        case ERR_EEPROM_I2C_INIT: return "EEPROM I2C init";
        case ERR_EEPROM_READ_FAIL: return "EEPROM read";
        case ERR_EEPROM_WRITE_FAIL: return "EEPROM write";
        case ERR_EEPROM_MUTEX_CREATE: return "EEPROM mutex";
        case ERR_EEPROM_HANDLER_ALLOC: return "EEPROM alloc";
        case ERR_EEPROM_INVALID_SIZE: return "EEPROM size";

        // Display
        case ERR_DISPLAY_MUTEX_CREATE: return "Display mutex";
        case ERR_DISPLAY_INIT_FAIL: return "Display init";
        case ERR_DISPLAY_TASK_CREATE: return "Display task";

        // Neopixel
        case ERR_NEOPIXEL_MUTEX_CREATE: return "Neopixel mutex";
        case ERR_NEOPIXEL_PIO_INIT: return "Neopixel PIO";

        // WiFi
        case ERR_WIFI_INIT_FAIL: return "WiFi init";
        case ERR_WIFI_CONNECT_FAIL: return "WiFi connect";
        case ERR_WIFI_QUEUE_CREATE: return "WiFi queue";
        case ERR_WIFI_TASK_CREATE: return "WiFi task";

        // Motors
        case ERR_MOTOR_UART_INIT: return "Motor UART";
        case ERR_MOTOR_DRIVER_ALLOC: return "Motor alloc";
        case ERR_MOTOR_COARSE_INIT: return "Coarse motor";
        case ERR_MOTOR_FINE_INIT: return "Fine motor";
        case ERR_MOTOR_QUEUE_CREATE: return "Motor queue";
        case ERR_MOTOR_TASK_CREATE: return "Motor task";

        // Scale
        case ERR_SCALE_UART_INIT: return "Scale UART";
        case ERR_SCALE_SEMAPHORE_CREATE: return "Scale sema";
        case ERR_SCALE_MUTEX_CREATE: return "Scale mutex";
        case ERR_SCALE_TASK_CREATE: return "Scale task";
        case ERR_SCALE_DRIVER_SELECT: return "Scale driver";

        // Servo
        case ERR_SERVO_QUEUE_CREATE: return "Servo queue";
        case ERR_SERVO_SEMAPHORE_CREATE: return "Servo sema";
        case ERR_SERVO_TASK_CREATE: return "Servo task";

        // Profile
        case ERR_PROFILE_EEPROM_READ: return "Profile read";
        case ERR_PROFILE_EEPROM_WRITE: return "Profile write";

        // Charge mode
        case ERR_CHARGE_EEPROM_READ: return "Charge read";
        case ERR_CHARGE_EEPROM_WRITE: return "Charge write";

        // REST
        case ERR_REST_QUEUE_CREATE: return "REST queue";
        case ERR_REST_ALLOC_FAIL: return "REST alloc";

        // Memory
        case ERR_MEMORY_ALLOC: return "Memory alloc";
        case ERR_MEMORY_BUFFER_OVERFLOW: return "Buffer overflow";

        // Calibration
        case ERR_CALIBRATE_TASK_CREATE: return "Calibrate task";

        default: return "Unknown";
    }
}

// Get error category from code (for serial output)
static const char* error_get_category(error_code_t code) {
    if (code >= 100 && code < 200) return "EEPROM";
    if (code >= 200 && code < 300) return "DISPLAY";
    if (code >= 300 && code < 400) return "LED";
    if (code >= 400 && code < 500) return "WIFI";
    if (code >= 500 && code < 600) return "MOTOR";
    if (code >= 600 && code < 700) return "SCALE";
    if (code >= 700 && code < 800) return "SERVO";
    if (code >= 800 && code < 900) return "PROFILE";
    if (code >= 900 && code < 1000) return "CHARGE";
    if (code >= 1000 && code < 1100) return "REST";
    if (code >= 1100 && code < 1200) return "MEMORY";
    if (code >= 1200 && code < 1300) return "CALIBRATE";
    return "UNKNOWN";
}

// Add error to log buffer
static void error_log_add(error_code_t code) {
    error_log[error_log_write_idx] = code;
    error_log_write_idx = (error_log_write_idx + 1) % ERROR_LOG_SIZE;
    if (error_log_count < ERROR_LOG_SIZE) {
        error_log_count++;
    }
}

// Update display with scrolling error list
static void error_update_display(void) {
    if (!error_state.display_ready) {
        return;
    }

    u8g2_t *display = get_display_handler();
    if (display == NULL) {
        return;
    }

    acquire_display_buffer_access();

    u8g2_ClearBuffer(display);

    // Title with error count
    u8g2_SetFont(display, u8g2_font_helvB08_tr);
    char title[20];
    snprintf(title, sizeof(title), "ERRORS (%d)", error_log_count);
    u8g2_DrawStr(display, 5, 10, title);

    // Line under title
    u8g2_DrawHLine(display, 0, ERROR_TITLE_HEIGHT, u8g2_GetDisplayWidth(display));

    // Show most recent errors (newest at bottom, scrolls up)
    u8g2_SetFont(display, u8g2_font_4x6_tf);  // Tiny fixed-width font, ~25 chars/row

    uint8_t lines_to_show = (error_log_count < ERROR_MAX_DISPLAY_LINES)
                            ? error_log_count
                            : ERROR_MAX_DISPLAY_LINES;

    // Calculate starting index for oldest error to display
    int start_idx;
    if (error_log_count <= ERROR_MAX_DISPLAY_LINES) {
        // Show all errors from beginning
        start_idx = 0;
    } else {
        // Show most recent ERROR_MAX_DISPLAY_LINES errors
        start_idx = error_log_count - ERROR_MAX_DISPLAY_LINES;
    }

    for (uint8_t i = 0; i < lines_to_show; i++) {
        // Get error from log (accounting for circular buffer)
        int log_idx;
        if (error_log_count < ERROR_LOG_SIZE) {
            log_idx = start_idx + i;
        } else {
            log_idx = (error_log_write_idx + start_idx + i) % ERROR_LOG_SIZE;
        }

        error_code_t code = error_log[log_idx];

        // Format: "EEP:105 EEPROM alloc"
        char line[32];
        snprintf(line, sizeof(line), "%s:%d %s",
                 error_get_short_category(code),
                 (int)code,
                 error_code_to_string(code));

        int y_pos = ERROR_TITLE_HEIGHT + 10 + (i * ERROR_LINE_HEIGHT);
        u8g2_DrawStr(display, 2, y_pos, line);
    }

    u8g2_SendBuffer(display);

    release_display_buffer_access();
}

void report_error(error_code_t code) {
    if (code == ERR_NONE) {
        return;
    }

    // Store error
    last_error = code;
    error_occurred = true;

    // Add to error log
    error_log_add(code);

    // 1. Always log to printf (USB serial) - available from boot
    printf("ERROR [%s] %d: %s\n",
           error_get_category(code),
           (int)code,
           error_code_to_string(code));

    // 2. Set LED to red if neopixel is ready
    if (error_state.neopixel_ready) {
        // Set all LEDs to red to indicate error
        _neopixel_led_set_colour(ERROR_LED_COLOR, ERROR_LED_COLOR, ERROR_LED_COLOR);
    }

    // 3. Update display with scrolling error list
    error_update_display();

    // 4. WiFi/REST notification could be added here
    // if (error_state.wifi_ready) { ... }
}

error_code_t error_get_last(void) {
    return last_error;
}

void error_clear_last(void) {
    last_error = ERR_NONE;
    error_occurred = false;
    error_log_count = 0;
    error_log_write_idx = 0;
}

bool error_has_occurred(void) {
    return error_occurred;
}

uint8_t error_get_count(void) {
    return error_log_count;
}

error_code_t error_get_at(uint8_t index) {
    if (index >= error_log_count) {
        return ERR_NONE;
    }

    // Calculate actual index in circular buffer
    int log_idx;
    if (error_log_count < ERROR_LOG_SIZE) {
        log_idx = index;
    } else {
        log_idx = (error_log_write_idx + index) % ERROR_LOG_SIZE;
    }

    return error_log[log_idx];
}
