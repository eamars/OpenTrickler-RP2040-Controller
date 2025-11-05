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
    SCALE_DRIVER_AND_FXI = 0,
    SCALE_DRIVER_STEINBERG_SBS = 1,
    SCALE_DRIVER_GNG_JJB = 2,
    SCALE_DRIVER_USSOLID_JFDBS = 3,
    SCALE_DRIVER_JM_SCIENCE = 4,
    SCALE_DRIVER_CREEDMOOR = 5,
    SCALE_DRIVER_RADWAG_PS_R2 = 6,
} scale_driver_t;


typedef enum {
    SCALE_ACTION_NO_ACTION = 0,
    SCALE_ACTION_FORCE_ZERO = 1,
} scale_action_t;


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