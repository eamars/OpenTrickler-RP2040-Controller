#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>


typedef enum {
    COARSE_TRICKLER_MOTOR,
    FINE_TRICKLER_MOTOR,
} motor_select_t;

typedef struct {
    uint32_t full_steps_per_rotation;
    uint16_t current_ma;
    uint16_t microsteps;
    bool direction;
    uint16_t max_speed_rpm;
} motor_motion_config_t;




typedef struct {
    float new_speed_setpoint;
    float direction;
    float ramp_rate;
    motor_select_t motor_select;
} stepper_speed_control_t;
extern QueueHandle_t stepper_speed_control_queue;


#endif