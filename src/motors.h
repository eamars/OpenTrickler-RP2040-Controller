#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "http_rest.h"

#define EEPROM_MOTOR_DATA_REV                     3              // 16 byte 


// Terms
// Velocity: speed with direction (clockwise or counter-clockwise)
// Speed: The magnitude of how fast the motor rotates, in rev/s

typedef enum {
    SELECT_COARSE_TRICKLER_MOTOR,
    SELECT_FINE_TRICKLER_MOTOR,
} motor_select_t;


typedef struct {
    float angular_acceleration;  // In rev/s^2
    uint32_t full_steps_per_rotation;
    uint16_t current_ma;
    uint16_t microsteps;
    uint16_t max_speed_rps;
    uint16_t r_sense;
    bool inverted_direction;
    bool inverted_enable;
} motor_persistent_config_t;


typedef struct {
    uint32_t motor_data_rev;
    motor_persistent_config_t motor_data[2];
} eeprom_motor_data_t;


typedef struct {
    // Setings that should be read from EEPROM
    motor_persistent_config_t persistent_config;

    // Constants to be copied from the hardware configuration
    uint dir_pin;
    uint en_pin;
    uint step_pin;
    uint8_t uart_addr;

    // Set at run time
    void * tmc_driver;
    int pio_sm;
    uint pio_program_offset;

    // Used to store some live data
    float prev_velocity;
    bool step_direction;

    // RTOS control
    TaskHandle_t stepper_speed_control_task_handler;
    QueueHandle_t stepper_speed_control_queue;
} motor_config_t;



// Interface functions
#ifdef __cplusplus
extern "C" {
#endif

bool motors_init(void);
bool motor_config_init(void);
bool motor_config_save(void);
void motor_task(void *p);
void motor_set_speed(motor_select_t selected_motor, float new_velocity);
uint16_t get_motor_max_speed(motor_select_t selected_motor);
void motor_enable(motor_select_t selected_motor, bool enable);

// REST interface
bool http_rest_coarse_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_fine_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_motor_speed(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}
#endif

#endif