/* Isolate the C and C++ */
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <math.h>
#include "app.h"
#include "tmc2209.h"
#include "hardware/uart.h"
#include "configuration.h"
#include "hardware/gpio.h"
#include "motors.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "stepper.pio.h"

typedef enum {
    MOVE_BACKWARD = 0,
    MOVE_FORWARD = 1,
    UNCHANGED = 2,
} stepper_direction_t;

typedef struct {
    float prev_speed;
    float next_speed;
    float ramp_rate;
    stepper_direction_t direction;
} stepper_speed_delta_control_t;
QueueHandle_t stepper_speed_delta_control_queue;

MotorControllerSelect_t coarse_motor_controller_select = USE_TMC2209;
MotorControllerSelect_t fine_motor_controller_select = USE_TMC2209;

TMC2209_t coarse_motor;
TMC2209_t fine_motor;

motor_motion_config_t coarse_motor_config;

QueueHandle_t stepper_speed_control_queue;
TaskHandle_t stepper_speed_control_task_handler = NULL;

float coarse_motor_speed_rps = 0.05; 




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
    coarse_motor_config.microsteps = 256;

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
    // gpio_put(COARSE_MOTOR_EN_PIN, 0);

    // gpio_init(COARSE_MOTOR_STEP_PIN);
    // gpio_set_dir(COARSE_MOTOR_STEP_PIN, GPIO_OUT);
    // gpio_put(COARSE_MOTOR_STEP_PIN, 1);

    gpio_init(COARSE_MOTOR_DIR_PIN);
    gpio_set_dir(COARSE_MOTOR_DIR_PIN, GPIO_OUT);
    gpio_put(COARSE_MOTOR_DIR_PIN, MOVE_FORWARD);

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

void pio_stepper_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}


uint32_t speed_to_period(float speed, uint32_t pio_clock_speed, uint32_t full_rotation_steps) {
    // uint32_t pio_clock_speed = clock_get_hz(clk_sys);

    // Step speed (step/s) is calculated by full rotation steps x rotations per second
    uint32_t delay_period;
    if (speed < 1e-3) {
        delay_period = 0;
    }
    else {
        float steps_speed = full_rotation_steps * speed;
        delay_period = (uint32_t) (pio_clock_speed / steps_speed);
    }

    // Discount the period when holds low
    if (delay_period >= 3) {
        delay_period -= 3;
    }
    else {
        delay_period = 0;
    }

    return delay_period;
}


void stepper_speed_control_task(void *p) {
    float prev_speed = 0;
    bool prev_direction = MOVE_FORWARD;

    // Currently doing speed control
    while (true) {
        // Wait for new speed
        stepper_speed_control_t new_setpoint;
        xQueueReceive(stepper_speed_control_queue, &new_setpoint, portMAX_DELAY);

        // If the direction has changed then generate two stepper control packet
        if (prev_direction != new_setpoint.direction) {
            // First: ramp down to 0
            stepper_speed_delta_control_t ramp_down;
            ramp_down.prev_speed = prev_speed;
            ramp_down.next_speed = 0;
            ramp_down.direction = UNCHANGED;
            ramp_down.ramp_rate = new_setpoint.ramp_rate;
            
            stepper_speed_delta_control_t ramp_up;
            ramp_up.prev_speed = 0;
            ramp_up.next_speed = new_setpoint.new_speed_setpoint;
            ramp_up.direction = new_setpoint.direction;
            ramp_up.ramp_rate = new_setpoint.ramp_rate;

            xQueueSend(stepper_speed_delta_control_queue, &ramp_down, portMAX_DELAY);
            xQueueSend(stepper_speed_delta_control_queue, &ramp_up, portMAX_DELAY);
        }
        else {
            stepper_speed_delta_control_t speed_change;
            speed_change.prev_speed = prev_speed;
            speed_change.next_speed = new_setpoint.new_speed_setpoint;
            speed_change.direction = UNCHANGED;
            speed_change.ramp_rate = new_setpoint.ramp_rate;
            xQueueSend(stepper_speed_delta_control_queue, &speed_change, portMAX_DELAY);
        }

        // Update record
        prev_speed = new_setpoint.new_speed_setpoint;
        if (new_setpoint.direction != UNCHANGED) {
            prev_direction = new_setpoint.direction;
        }
        
    }
}


void motor_task(void *p) {
    bool status = motors_init();

    stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));
    stepper_speed_delta_control_queue = xQueueCreate(2, sizeof(stepper_speed_delta_control_t));

    UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
    xTaskCreate(stepper_speed_control_task, "Stepper Speed Control Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority + 1, &stepper_speed_control_task_handler);


    uint stepper_sm = pio_claim_unused_sm(pio0, true);
    uint stepper_offset = pio_add_program(pio0, &stepper_program);
    stepper_program_init(pio0, stepper_sm, stepper_offset, COARSE_MOTOR_STEP_PIN);
    pio_sm_set_enabled(pio0, stepper_sm, true);

    uint32_t full_rotation_steps = coarse_motor_config.full_steps_per_rotation * coarse_motor_config.microsteps;
    uint32_t pio_speed = clock_get_hz(clk_sys);

    while (true) {
        // Wait for new speed
        stepper_speed_delta_control_t new_delta;
        xQueueReceive(stepper_speed_delta_control_queue, &new_delta, portMAX_DELAY);

        // Change speed if needed
        if (new_delta.direction != UNCHANGED) {
            gpio_put(COARSE_MOTOR_DIR_PIN, new_delta.direction);
        }

        // Calculate ramp param
        float v0 = new_delta.prev_speed;
        float v1 = new_delta.next_speed;
        float dv = v1 - v0;
        float ramp_time_s = fabs(dv / new_delta.ramp_rate);

        // Calculate termination condition
        uint32_t ramp_time_us = (uint32_t) (fabs(ramp_time_s) * 1e6);
        uint64_t start_time = time_us_64();
        uint64_t stop_time = start_time + ramp_time_us;

        float current_speed;
        uint32_t current_period;
        while (true) {
            uint64_t current_time = time_us_64();
            if (current_time > stop_time) {
                break;
            }

            float percentage = (current_time - start_time) / (float) ramp_time_us;

            current_speed = v0 + dv * percentage;
            current_period = speed_to_period(current_speed, pio_speed, full_rotation_steps);
            pio_sm_put_blocking(pio0, stepper_sm, current_period);
        }

        current_period = speed_to_period(v1, pio_speed, full_rotation_steps);
        pio_sm_put_blocking(pio0, stepper_sm, current_period);
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
