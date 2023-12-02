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
    BAUDRATE_4800,
    BAUDRATE_9600,
    BAUDRATE_19200,
} scale_baudrate_t;


typedef enum {
    SCALE_DRIVER_AND_FXI = 0,
    SCALE_DRIVER_STEINBERG_SBS,
} scale_driver_t;


typedef struct {
    uint16_t scale_data_rev;
    scale_driver_t scale_driver;
    scale_baudrate_t scale_baudrate;
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
bool scale_block_wait_for_next_measurement(uint32_t block_time_ms, float * current_measurement);

void set_scale_driver(scale_driver_t scale_driver);

const char * get_scale_driver_string();

bool scale_config_save(void);

void scale_press_re_zero_key(void);
void scale_press_print_key(void);
void scale_press_sample_key(void);
void scale_press_mode_key(void);
void scale_press_cal_key(void);
void scale_press_on_off_key(void);

// Low lever handler for writing data to the scale
void scale_write(char * command, size_t len);

// REST
bool http_rest_scale_weight(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_scale_config(struct fs_file *file, int num_params, char *params[], char *values[]);


// Features
uint8_t scale_calibrate_with_external_weight();
AppState_t scale_enable_fast_report(AppState_t prev_state);


#ifdef __cplusplus
}
#endif


#endif