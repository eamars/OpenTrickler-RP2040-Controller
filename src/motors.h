#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>
#include <FreeRTOS.h>

// Terms
// Velocity: speed with direction (clockwise or counter-clockwise)
// Speed: The magnitude of how fast the motor rotates, in rev/s

typedef enum {
    SELECT_COARSE_TRICKLER_MOTOR,
    SELECT_FINE_TRICKLER_MOTOR,
} motor_select_t;

typedef struct {
    // control flag
    bool is_initialized;

    // Constants to be copied from the hardware configuration
    uint dir_pin;
    uint en_pin;
    uint step_pin;

    // Setings that should be read from EEPROM
    float angular_acceleration;  // In rev/s^2
    uint32_t full_steps_per_rotation;
    uint16_t current_ma;
    uint16_t microsteps;
    uint16_t max_speed_rps;
    uint16_t uart_addr;
    uint16_t r_sense;

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



typedef struct {
    float new_speed_setpoint;
    float direction;
    float ramp_rate;
} stepper_speed_control_t;
extern QueueHandle_t stepper_speed_control_queue;


// Interface functions
#ifdef __cplusplus
extern "C" {
#endif

    void motor_task(void *p);
    void motor_set_speed(motor_select_t selected_motor, float new_velocity);
    void motor_enable(motor_select_t selected_motor, bool enable);

#ifdef __cplusplus
}
#endif

#endif