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

TMC2209_t fine_motor;

uart_inst_t * uart_map[2] = {
    uart0,
    uart1,
};


void motors_init() {
    // TODO: USE motor select

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(FINE_MOTOR_UART, 500000);
    gpio_set_function(FINE_MOTOR_UART_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(FINE_MOTOR_UART_TX_PIN, GPIO_FUNC_UART);

    TMC2209_SetDefaults(&fine_motor);
    fine_motor.config.motor.id = 1;
    fine_motor.config.motor.address = 0;
    fine_motor.config.current = 0.5;
    fine_motor.config.microsteps = 256;

    // TMC2209_ReadRegister(&fine_motor, (TMC2209_datagram_t *)&fine_motor.drv_status);
    // printf("Motor Status: %x\n", fine_motor.drv_status.reg.value);
}


void tmc_uart_write (trinamic_motor_t driver, TMC_uart_write_datagram_t *datagram)
{
    uart_inst_t * target_uart = uart_map[driver.id];

    uart_write_blocking(target_uart, datagram->data, sizeof(TMC_uart_write_datagram_t));
}

TMC_uart_write_datagram_t *tmc_uart_read (trinamic_motor_t driver, TMC_uart_read_datagram_t *datagram)
{
    static TMC_uart_write_datagram_t wdgr = {0}; 
    uart_inst_t * target_uart = uart_map[driver.id];

    uart_write_blocking(target_uart, datagram->data, sizeof(TMC_uart_read_datagram_t));

    if (uart_is_readable_within_us(target_uart, 1000)) {
        uart_read_blocking(target_uart, wdgr.data, 8);
    }
    else {
        wdgr.msg.addr.value = 0xFF;
    }

    return &wdgr;
}
