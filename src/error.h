#ifndef ERROR_H_
#define ERROR_H_

#include <stdbool.h>
#include <stdint.h>

// Error codes for all modules
// Format: ERR_<MODULE>_<SPECIFIC_ERROR>
typedef enum {
    ERR_NONE = 0,

    // EEPROM errors (1xx)
    ERR_EEPROM_I2C_INIT = 100,
    ERR_EEPROM_READ_FAIL,
    ERR_EEPROM_WRITE_FAIL,
    ERR_EEPROM_MUTEX_CREATE,
    ERR_EEPROM_HANDLER_ALLOC,
    ERR_EEPROM_INVALID_SIZE,

    // Display errors (2xx)
    ERR_DISPLAY_MUTEX_CREATE = 200,
    ERR_DISPLAY_INIT_FAIL,
    ERR_DISPLAY_TASK_CREATE,

    // Neopixel LED errors (3xx)
    ERR_NEOPIXEL_MUTEX_CREATE = 300,
    ERR_NEOPIXEL_PIO_INIT,

    // Wireless errors (4xx)
    ERR_WIFI_INIT_FAIL = 400,
    ERR_WIFI_CONNECT_FAIL,
    ERR_WIFI_QUEUE_CREATE,
    ERR_WIFI_TASK_CREATE,

    // Motor errors (5xx)
    ERR_MOTOR_UART_INIT = 500,
    ERR_MOTOR_DRIVER_ALLOC,
    ERR_MOTOR_COARSE_INIT,
    ERR_MOTOR_FINE_INIT,
    ERR_MOTOR_QUEUE_CREATE,
    ERR_MOTOR_TASK_CREATE,

    // Scale errors (6xx)
    ERR_SCALE_UART_INIT = 600,
    ERR_SCALE_SEMAPHORE_CREATE,
    ERR_SCALE_MUTEX_CREATE,
    ERR_SCALE_TASK_CREATE,
    ERR_SCALE_DRIVER_SELECT,

    // Servo gate errors (7xx)
    ERR_SERVO_QUEUE_CREATE = 700,
    ERR_SERVO_SEMAPHORE_CREATE,
    ERR_SERVO_TASK_CREATE,

    // Profile errors (8xx)
    ERR_PROFILE_EEPROM_READ = 800,
    ERR_PROFILE_EEPROM_WRITE,

    // Charge mode errors (9xx)
    ERR_CHARGE_EEPROM_READ = 900,
    ERR_CHARGE_EEPROM_WRITE,

    // REST API errors (10xx)
    ERR_REST_QUEUE_CREATE = 1000,
    ERR_REST_ALLOC_FAIL,

    // Memory errors (11xx)
    ERR_MEMORY_ALLOC = 1100,
    ERR_MEMORY_BUFFER_OVERFLOW,

    // Calibration errors (12xx)
    ERR_CALIBRATE_TASK_CREATE = 1200,

    // Generic/unknown
    ERR_UNKNOWN = 9999,
} error_code_t;

// Initialization state flags (what's available for error reporting)
typedef struct {
    bool eeprom_ready;
    bool neopixel_ready;
    bool display_ready;
    bool wifi_ready;
} error_system_state_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize error system (call early in main)
void error_system_init(void);

// Mark subsystems as ready for error reporting
void error_set_eeprom_ready(bool ready);
void error_set_neopixel_ready(bool ready);
void error_set_display_ready(bool ready);
void error_set_wifi_ready(bool ready);

// Main error reporting function
// Reports via all available channels (printf, LED, display, etc.)
void report_error(error_code_t code);

// Get human-readable error string
const char* error_code_to_string(error_code_t code);

// Get last error (for REST API queries)
error_code_t error_get_last(void);

// Clear last error
void error_clear_last(void);

// Check if system has any errors
bool error_has_occurred(void);

// Get error count
uint8_t error_get_count(void);

// Get error at index (0 = oldest in buffer)
error_code_t error_get_at(uint8_t index);

// Maximum errors stored in log
#define ERROR_LOG_SIZE 8

#ifdef __cplusplus
}
#endif

#endif // ERROR_H_
