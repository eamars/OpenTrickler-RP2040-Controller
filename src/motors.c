/* Isolate the C and C++ */
#include <stddef.h>
#include <stdio.h>
#include "app.h"
#include "tmc2209.h"
#include "hardware/uart.h"
#include "configuration.h"
#include "hardware/gpio.h"

MotorControllerSelect_t coarse_motor_controller_select = USE_TMC2209;
MotorControllerSelect_t fine_motor_controller_select = USE_TMC2209;

TMC2209_t coarse_motor;
TMC2209_t fine_motor;

void motors_init() {
    // TODO: USE motor select

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 500000);
    gpio_set_function(MOTOR_UART_RX, GPIO_FUNC_UART);
    gpio_set_function(MOTOR_UART_TX, GPIO_FUNC_UART);

    TMC2209_SetDefaults(&fine_motor);
    fine_motor.config.motor.id = 1;
    fine_motor.config.motor.address = 1;
    fine_motor.config.current = 0.5;
    fine_motor.config.microsteps = 256;

    TMC2209_SetDefaults(&coarse_motor);
    coarse_motor.config.motor.id = 0;
    coarse_motor.config.motor.address = 0;
    coarse_motor.config.current = 0.5;
    coarse_motor.config.microsteps = 256;

    // TMC2209_ReadRegister(&fine_motor, (TMC2209_datagram_t *)&fine_motor.drv_status);
    // printf("Motor Status: %x\n", fine_motor.drv_status.reg.value);
}


void tmc_uart_write (trinamic_motor_t driver, TMC_uart_write_datagram_t *datagram)
{
    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_write_datagram_t));
}

TMC_uart_write_datagram_t *tmc_uart_read (trinamic_motor_t driver, TMC_uart_read_datagram_t *datagram)
{
    static TMC_uart_write_datagram_t wdgr = {0}; 

    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_read_datagram_t));

    if (uart_is_readable_within_us(MOTOR_UART, 1000)) {
        uart_read_blocking(MOTOR_UART, wdgr.data, 8);
    }
    else {
        wdgr.msg.addr.value = 0xFF;
    }

    return &wdgr;
}
