#ifndef SCALE_H_
#define SCALE_H_

#include <FreeRTOS.h>
#include <stdint.h>

#include "app.h"
#include "http_rest.h"
#include <semphr.h>

#define EEPROM_SCALE_DATA_REV                     3              // 16 byte 


// Abstracted base class
typedef struct {
    // Basic functions
    void (*read_loop_task)(void *self);
    void (*force_zero)(void);
} scale_handle_t;


typedef enum {
    BAUDRATE_4800 = 0,
    BAUDRATE_9600 = 1,
    BAUDRATE_19200 = 2,
} scale_baudrate_t;


typedef enum {
    UART_FMT_8D_1S_NP = 0,
    UART_FMT_7D_1S_NP = 1,
} scale_uart_format_t;


typedef enum {
    SCALE_DRIVER_AND_FXI = 0,
    SCALE_DRIVER_STEINBERG_SBS = 1,
    SCALE_DRIVER_GNG_JJB = 2,
    SCALE_DRIVER_USSOLID_JFDBS = 3,
    SCALE_DRIVER_JM_SCIENCE = 4,
    SCALE_DRIVER_CREEDMOOR = 5,
    SCALE_DRIVER_RADWAG_PS_R2 = 6,
    SCALE_DRIVER_SARTORIUS = 7,
    SCALE_DRIVER_GENERIC_DRV = 8,
    SCALE_DRIVER_SIMULATED = 9,
} scale_driver_t;


typedef enum {
    SCALE_ACTION_NO_ACTION = 0,
    SCALE_ACTION_FORCE_ZERO = 1,
} scale_action_t;


typedef struct {
    uint16_t scale_data_rev;
    scale_driver_t scale_driver;
    scale_baudrate_t scale_baudrate;
    scale_uart_format_t scale_uart_format;
} eeprom_scale_data_t;


typedef struct {
    eeprom_scale_data_t persistent_config;
    scale_handle_t * scale_handle;
    SemaphoreHandle_t scale_measurement_ready;
    SemaphoreHandle_t scale_serial_write_access_mutex;
    float current_scale_measurement;
} scale_config_t;


#ifdef __cplusplus
extern "C" {
#endif

// Scale related calls
bool scale_init();

float scale_get_current_measurement();

/**
 * @brief The one way a reading is published (ADR-013). Every scale driver and the simulated scale call it, in place
 * of writing the measurement themselves. A driver says a frame did not parse by giving NaN. A value that is not a
 * finite number, NaN or an infinity, is not published: the reading is not changed, the charge is not told of a new
 * one, and it is counted (scale_get_bad_frames()). So a bad frame is a frame that never came.
 */
void scale_publish_measurement(float weight);
uint32_t scale_get_bad_frames(void);

/**
 * @brief The scale's usual time between readings, in ms: the median of its last 16 published readings' intervals,
 * so each scale's own rhythm (ADR-013). 0 until SCALE_RHYTHM_MIN_INTERVALS are known; a charge waits for ten steady
 * readings before it pours, so by then it is known.
 */
#define SCALE_RHYTHM_MIN_INTERVALS 4
uint32_t scale_get_usual_interval_ms(void);

/**
 * @brief The remote control's simulated scale (ADR-001): main's simulated driver stands in for the stored one
 * until scale_simulation_stop() or a restart. Nothing is stored.
 * @return false when the stored driver is already the simulated one, or its task could not start.
 */
bool scale_simulation_start(void);
void scale_simulation_stop(void);
bool scale_simulation_is_on(void);
bool scale_block_wait_for_next_measurement(uint32_t block_time_ms, float * current_measurement);

/**
 * @brief The remote control's frame tap (ADR-013): while it is on, a driver that calls scale_tap_frame() copies
 * each frame it decoded to the console, as it came and with the value it made of it. Nothing is stored.
 */
void scale_tap_set(bool on);
bool scale_tap_is_on(void);

/**
 * @brief Faults the remote control puts into the simulated scale (ADR-013); nothing is stored.
 * simulated_scale_set_rate(): readings a second, 1 to 50 (100 ms between readings until set); false if out of range.
 * simulated_scale_send_bad(): its next reading is a frame that did not parse, published as the drivers do.
 * simulated_scale_go_quiet(): nothing published for that many ms; its powder keeps falling meanwhile.
 */
bool simulated_scale_set_rate(float per_second);
float simulated_scale_get_rate(void);
void simulated_scale_send_bad(void);
void simulated_scale_go_quiet(uint32_t ms);

/**
 * @brief Print one frame as "@sf <ms> <value> <bytes>" when the tap is on: the time since boot in ms, the value
 * the driver made of it, and the frame's bytes in hex. Does nothing when the tap is off.
 */
void scale_tap_frame(const uint8_t * bytes, size_t len, float value);

void set_scale_driver(scale_driver_t scale_driver);

const char * get_scale_driver_string();

bool scale_config_save(void);

// Low lever handler for writing data to the scale
void scale_write(const char * command, size_t len);

// REST
bool http_rest_scale_action(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_scale_config(struct fs_file *file, int num_params, char *params[], char *values[]);


// Features
uint8_t scale_calibrate_with_external_weight();
AppState_t scale_enable_fast_report(AppState_t prev_state);


#ifdef __cplusplus
}
#endif


#endif