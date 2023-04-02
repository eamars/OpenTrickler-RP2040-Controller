/* Isolate the C and C++ */
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include "app.h"
#include "tmc2209.h"
#include "hardware/uart.h"
#include "configuration.h"
#include "hardware/gpio.h"
#include "motors.h"

MotorControllerSelect_t coarse_motor_controller_select = USE_TMC2209;
MotorControllerSelect_t fine_motor_controller_select = USE_TMC2209;

TMC2209_t coarse_motor;
TMC2209_t fine_motor;

motor_motion_config_t coarse_motor_config;

float coarse_motor_speed_rps = 0.1; 


void _enable_uart_rx(uart_inst_t * uart, bool enable) {
    if (enable) {
        hw_set_bits(&uart_get_hw(uart)->cr, UART_UARTCR_RXE_BITS);
    }
    else {
        hw_clear_bits(&uart_get_hw(uart)->cr, UART_UARTCR_RXE_BITS);
    }
}


void _clear_rx_buffer(uart_inst_t * uart) {
    while(!(uart_get_hw(uart)->fr & UART_UARTFR_RXFE_BITS)) {
        uart_get_hw(uart)->dr;
    }
}


bool _block_wait_for_sync(uart_inst_t * uart) {
    uint8_t c;
    do
    {
        if (uart_is_readable_within_us(uart, 2000)) {
            uart_read_blocking(MOTOR_UART, &c, 1);
        }
        else {
            return false;
        }
    } while (c != 0x05); 

    return true;
}



bool motors_init() {
    // TODO: Define motor configs elsewhere
    coarse_motor_config.current_ma = 500;
    coarse_motor_config.direction = true;
    coarse_motor_config.full_steps_per_rotation = 200;
    coarse_motor_config.max_speed_rpm = 2000;
    coarse_motor_config.microsteps = 16;

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 115200);
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
    coarse_motor.config.current = coarse_motor_config.current_ma;
    coarse_motor.config.r_sense = 110;
    coarse_motor.config.hold_current_pct = 100;
    coarse_motor.config.microsteps = coarse_motor_config.microsteps;

    TMC2209_Init(&coarse_motor);

    return true;
}

void motor_task(void *p) {
    bool status = motors_init();

    while (true) {
        absolute_time_t current_time = get_absolute_time();
        gpio_put(COARSE_MOTOR_STEP_PIN, 1);
        gpio_put(COARSE_MOTOR_STEP_PIN, 0);

        uint32_t full_rotation_steps = coarse_motor_config.full_steps_per_rotation * coarse_motor_config.microsteps;
        uint32_t step_time_us = (int) round(1000 * 1000 / (coarse_motor_speed_rps * full_rotation_steps));


        sleep_until(delayed_by_us(current_time, step_time_us));
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

    // Read until 0x05 is received
    if (_block_wait_for_sync(MOTOR_UART)) {
        // Read full payload
        wdgr.data[0] = 0x05;
        uart_read_blocking(MOTOR_UART, &wdgr.data[1], 7);

        _enable_uart_rx(MOTOR_UART, false);
    }
    else {
        wdgr.msg.addr.value = 0xFF;
    }
    

    return &wdgr;
}
