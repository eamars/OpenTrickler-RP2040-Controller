#ifndef MOTORS_H_
#define MOTORS_H_

#include <stdint.h>

typedef struct {
    uint32_t full_steps_per_rotation;
    uint16_t current_ma;
    uint16_t microsteps;
    bool direction;
    uint16_t max_speed_rpm;
} motor_motion_config_t;


typedef struct {
    float speed;
    float ramp_rate;
} stepper_speed_setpoint_t;


#endif