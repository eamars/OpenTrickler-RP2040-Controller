/* Isolate the C and C++ */
#include <stddef.h>
#include "app.h"
#include "hardware/uart.h"
#include "configuration.h"
#include "hardware/gpio.h"

MotorControllerSelect_t coarse_motor_controller_select = USE_TMC2209;
MotorControllerSelect_t fine_motor_controller_select = USE_TMC2209;


void motors_init() {
    ;
}