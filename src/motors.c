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


void _enable_uart_rx(uart_inst_t * uart, bool enable) {
    if (enable) {
        hw_clear_bits(&uart_get_hw(uart)->cr, UART_UARTCR_RXE_BITS);
    }
    else {
        hw_set_bits(&uart_get_hw(uart)->cr, UART_UARTCR_RXE_BITS);
    }
}


void _clear_rx_buffer(uart_inst_t * uart) {
    while(!(uart_get_hw(uart)->fr & UART_UARTFR_RXFE_BITS)) {
        uart_get_hw(uart)->dr;
    }
}



bool motors_init() {
    // TODO: USE motor select

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 500000);
    gpio_set_function(MOTOR_UART_RX, GPIO_FUNC_UART);
    gpio_set_function(MOTOR_UART_TX, GPIO_FUNC_UART);

    _enable_uart_rx(MOTOR_UART, false);

    // Initialize fine motor
    gpio_init(FINE_MOTOR_EN_PIN);
    gpio_set_dir(FINE_MOTOR_EN_PIN, GPIO_OUT);
    gpio_put(FINE_MOTOR_EN_PIN, 1);

    gpio_init(FINE_MOTOR_STEP_PIN);
    gpio_set_dir(FINE_MOTOR_STEP_PIN, GPIO_OUT);
    // gpio_put(FINE_MOTOR_STEP_PIN, 0);

    gpio_init(FINE_MOTOR_DIR_PIN);
    gpio_set_dir(FINE_MOTOR_DIR_PIN, GPIO_OUT);
    // gpio_put(FINE_MOTOR_DIR_PIN, 0);

    TMC2209_SetDefaults(&fine_motor);
    fine_motor.config.motor.id = 1;
    fine_motor.config.motor.address = 1;
    fine_motor.config.current = 0.5;
    fine_motor.config.microsteps = 256;

    // Initialize coarse motor
    gpio_init(COARSE_MOTOR_EN_PIN);
    gpio_set_dir(COARSE_MOTOR_EN_PIN, GPIO_OUT);
    gpio_put(COARSE_MOTOR_EN_PIN, 0);

    gpio_init(COARSE_MOTOR_STEP_PIN);
    gpio_set_dir(COARSE_MOTOR_STEP_PIN, GPIO_OUT);
    // gpio_put(COARSE_MOTOR_STEP_PIN, 0);

    gpio_init(COARSE_MOTOR_DIR_PIN);
    gpio_set_dir(COARSE_MOTOR_DIR_PIN, GPIO_OUT);
    // gpio_put(COARSE_MOTOR_DIR_PIN, 0);

    TMC2209_SetDefaults(&coarse_motor);
    coarse_motor.config.motor.id = 0;
    coarse_motor.config.motor.address = 0;
    coarse_motor.config.current = 0.5;
    coarse_motor.config.microsteps = 256;

    if (!TMC2209_ReadRegister(&coarse_motor, (TMC2209_datagram_t *)&coarse_motor.gstat)) {
        return false;
    }

    // TMC2209_WriteRegister(&coarse_motor, (TMC2209_datagram_t *)&coarse_motor.gstat);
    // TMC2209_ReadRegister(&coarse_motor, (TMC2209_datagram_t *)&coarse_motor.gconf);

    return true;
}

void motor_task(void *p) {
    bool status = motors_init();
    while (true) {
        ;
    }
}


void tmc_uart_write (trinamic_motor_t driver, TMC_uart_write_datagram_t *datagram)
{
    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_write_datagram_t));
}

TMC_uart_write_datagram_t *tmc_uart_read (trinamic_motor_t driver, TMC_uart_read_datagram_t *datagram)
{
    static TMC_uart_write_datagram_t wdgr = {0}; 

    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_read_datagram_t));
    _enable_uart_rx(MOTOR_UART, true);

    // Clear the buffer
    // _clear_rx_buffer(MOTOR_UART);

    // // Wait for response
    sleep_ms(1);

    uint8_t read_byte = 0;
    for (; read_byte < 8; read_byte++) {
        if (uart_is_readable_within_us(MOTOR_UART, 2000)) {
            uart_read_blocking(MOTOR_UART, &wdgr.data[read_byte], 1);
        }
        else {
            break;
        }
    }

    _enable_uart_rx(MOTOR_UART, false);

    if (read_byte < 7) {
        wdgr.msg.addr.value = 0xFF;
    }

    return &wdgr;
}
