#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "common.h"
#include "http_rest.h"
#include "step_pulses.h"

#define EEPROM_MOTOR_DATA_REV                     5              // 16 byte 


// Terms
// Velocity: speed with direction (clockwise or counter-clockwise)
// Speed: The magnitude of how fast the motor rotates, in rev/s

typedef enum {
    SELECT_COARSE_TRICKLER_MOTOR = 0,
    SELECT_FINE_TRICKLER_MOTOR,
    SELECT_BOTH_MOTOR,
    NUM_OF_MOTOR_SELECT,
} motor_select_t;

// One line of a driver's registers, without a line ending (motor_driver_dump)
typedef void (*motor_driver_line_fn)(const char *line, void *ctx);

// What the console shows of a driver and its serial line, counted since start-up
typedef struct {
    bool ready;                 // The driver answered and took every register
    uint16_t current_ma;        // The run current its registers give, mA RMS
    uint32_t writes;            // Writes the driver was seen to take
    uint32_t write_retries;     // Tries of a write its count did not confirm
    uint32_t read_retries;      // Tries of a read that brought no good answer
    uint32_t failures;          // Reads and writes given up
} motor_driver_stats_t;


typedef struct {
    uint32_t full_steps_per_rotation;
    uint16_t current_ma;
    uint16_t microsteps;
    uint16_t max_speed_rps;
    uint16_t r_sense;

    float angular_acceleration;  // In rev/s^2
    float min_speed_rps;
    float gear_ratio;

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
    uint8_t pulse_channel;      // Its channel of step_pulses

    // Live data, in the motor's own rev/s, before the gear
    volatile float target_velocity;     // Set by anyone; the motors' task takes it from here
    float velocity;                     // The speed now, ramps included: the motors' task's alone
    bool step_direction;                // The direction pin's level
} motor_config_t;



// Interface functions
#ifdef __cplusplus
extern "C" {
#endif

void motors_init(void);

/*
    A motor driver that does not answer, or settings that could not be read, do not stop the unit (ADR-012): the
    modes ask before they turn a motor.
    motors_ready() tries again a driver that did not answer; true when both have their registers.
    motors_get_fault() names what is missing in a few words for the screen, NULL when nothing is.
*/
bool motors_ready(void);
const char * motors_get_fault(void);

// For the console: a driver set up again from the stored settings, its registers by field, its line's counts
bool motor_driver_setup(motor_select_t selected_motor);
bool motor_driver_dump(motor_select_t selected_motor, motor_driver_line_fn emit, void * ctx);
void motor_driver_get_stats(motor_select_t selected_motor, motor_driver_stats_t * stats);

// The steps made and fed, and the rate the pulses were last fed at, in steps a second
void motor_get_step_stats(motor_select_t selected_motor, step_pulses_stats_t * stats);
float motor_get_step_rate(motor_select_t selected_motor);

// For a fault handler: the pulses and the drivers off, by register writes alone
void motors_fault_stop(void);

// For proofs, never stored: a driver made to look absent (asked at an address nobody has), and found again
void motor_driver_set_lost(motor_select_t selected_motor, bool lost);
bool motor_config_init(void);
bool motor_config_save(void);
void motor_set_speed(motor_select_t selected_motor, float new_velocity);
float motor_get_velocity(motor_select_t selected_motor);        // the speed now, ramps included, in the trickler's rev/s
float motor_get_shaft_velocity(motor_select_t selected_motor);  // the same, in the motor's own rev/s, before the gear
uint16_t get_motor_max_speed(motor_select_t selected_motor);
float get_motor_min_speed(motor_select_t selected_motor);
void motor_enable(motor_select_t selected_motor, bool enable);
const char * get_motor_select_string(motor_select_t selected_motor);

// REST interface
bool http_rest_coarse_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_fine_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}
#endif

#endif