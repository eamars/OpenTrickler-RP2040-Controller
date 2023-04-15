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


// Configurations
motor_config_t coarse_trickler_motor_config;
motor_config_t fine_trickler_motor_config;


const motor_config_t default_motor_config = {
    .angular_acceleration = 2000,       // 2000 rev/s^2
    .current_ma = 500,                  // 500 mA
    .full_steps_per_rotation = 200,     // 200: 1.8 deg stepper, 400: 0.9 deg stepper
    .max_speed_rps = 20,                // Maximum speed before the stepper runs out
    .microsteps = 256,                  // Default to maximum that the driver supports
    .r_sense = 110,                     // 0.110 ohm sense resistor the stepper driver
    .uart_addr = 0,                     // 0 to 4
};


// UART Control functions
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


void tmc_uart_write (trinamic_motor_t driver, TMC_uart_write_datagram_t *datagram)
{
    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_write_datagram_t));
}

TMC_uart_write_datagram_t *tmc_uart_read (trinamic_motor_t driver, TMC_uart_read_datagram_t *datagram)
{
    static TMC_uart_write_datagram_t wdgr = {0}; 

    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_read_datagram_t));

    sleep_us(20);

    _enable_uart_rx(MOTOR_UART, true);

    sleep_ms(2);

    // Read until 0x05 is received
    if (_block_wait_for_sync(MOTOR_UART)) {
        // Read full payload
        wdgr.data[0] = 0x05;
        uart_read_blocking(MOTOR_UART, &wdgr.data[1], 7);
    }
    else {
        wdgr.msg.addr.value = 0xFF;
    }

    // Read remaining data
    while (uart_is_readable(MOTOR_UART)) {
        uint8_t c;
        uart_read_blocking(MOTOR_UART, &c, 1);
    }
    
    _enable_uart_rx(MOTOR_UART, false);

    return &wdgr;
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


bool driver_init(motor_config_t * motor_config) {
    // Copy the user config to the driver config and initialize the communication
    // Return True if the initialization is successful, False otherwise. 
    // This must be called after the UART is initialized. 
    
    // Check if the tmc driver is already initialized. If true then free the previously allocated
    //  driver then re-create. 
    if (motor_config->tmc_driver) {
        free(motor_config->tmc_driver);
    }


    TMC2209_t * tmc_driver = malloc(sizeof(TMC2209_t));
    if (tmc_driver == NULL) {
        return false;
    }

    TMC2209_SetDefaults(tmc_driver);

    // Apply user configurations
    tmc_driver->config.motor.address = motor_config->uart_addr;
    tmc_driver->config.current = motor_config->current_ma;
    tmc_driver->config.r_sense = motor_config->r_sense;
    tmc_driver->config.hold_current_pct = 50;
    tmc_driver->config.microsteps = motor_config->microsteps;

    bool is_ok = TMC2209_Init(tmc_driver);

    motor_config->tmc_driver = (void *) tmc_driver;

    return is_ok;
}

bool driver_io_init(motor_config_t * motor_config) {
    gpio_init(motor_config->en_pin);
    gpio_set_dir(motor_config->en_pin, GPIO_OUT);
    gpio_put(motor_config->en_pin, 1);  // off at high

    // Note: STEP PIN is controlled by PIO block
    // gpio_init(motor_config->step_pin);
    // gpio_set_dir(motor_config->step_pin, GPIO_OUT);
    // gpio_put(motor_config->step_pin, 1);

    gpio_init(motor_config->dir_pin);
    gpio_set_dir(motor_config->dir_pin, GPIO_OUT);
    gpio_put(motor_config->dir_pin, motor_config->step_direction);

    return true;
}

bool driver_pio_init(motor_config_t * motor_config) {
    // Allocate PIO to the stepper
    motor_config->pio_sm = pio_claim_unused_sm(MOTOR_STEPPER_PIO, true);
    motor_config->pio_program_offset = pio_add_program(MOTOR_STEPPER_PIO, &stepper_program);
    stepper_program_init(MOTOR_STEPPER_PIO, 
                         motor_config->pio_sm, 
                         motor_config->pio_program_offset, motor_config->step_pin);
    // Start stepper state machine
    pio_sm_set_enabled(MOTOR_STEPPER_PIO, motor_config->pio_sm, true);

    return true;
}


bool motors_init(void) {
    bool is_ok;

    // TODO: Decide when to load from EEPROM 
    memcpy(&coarse_trickler_motor_config, &default_motor_config, sizeof(motor_config_t));
    memcpy(&fine_trickler_motor_config, &default_motor_config, sizeof(motor_config_t));

    coarse_trickler_motor_config.uart_addr = 0;
    coarse_trickler_motor_config.dir_pin = COARSE_MOTOR_DIR_PIN;
    coarse_trickler_motor_config.en_pin = COARSE_MOTOR_EN_PIN;
    coarse_trickler_motor_config.step_pin = COARSE_MOTOR_STEP_PIN;

    fine_trickler_motor_config.uart_addr = 1;
    fine_trickler_motor_config.dir_pin = FINE_MOTOR_DIR_PIN;
    fine_trickler_motor_config.en_pin = FINE_MOTOR_EN_PIN;
    fine_trickler_motor_config.step_pin = FINE_MOTOR_STEP_PIN;

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 115200);
    gpio_set_function(MOTOR_UART_RX, GPIO_FUNC_UART);
    gpio_set_function(MOTOR_UART_TX, GPIO_FUNC_UART);

    _enable_uart_rx(MOTOR_UART, false);

    // 
    // Enable coarse trickler motor at UART ADDR 0
    // 
    driver_io_init(&coarse_trickler_motor_config);

    // Allocate PIO to the stepper
    driver_pio_init(&coarse_trickler_motor_config);

    // Initialize the stepper driver 
    is_ok = driver_init(&coarse_trickler_motor_config);

    // 
    // Initialize fine trickler motor at UART ADDR 1
    // 
    driver_io_init(&fine_trickler_motor_config);

    // Allocate PIO to the stepper
    driver_pio_init(&fine_trickler_motor_config);
    
    // Initialize the stepper driver
    is_ok = driver_init(&fine_trickler_motor_config);

    return true;
}


void speed_ramp(motor_config_t * motor_config, float prev_speed, float new_speed, uint32_t pio_speed) {
    // Calculate ramp param
    float dv = new_speed - prev_speed;
    float ramp_time_s = fabs(dv / motor_config->angular_acceleration);
    uint32_t full_rotation_steps = motor_config->full_steps_per_rotation * motor_config->microsteps;

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

        current_speed = prev_speed + dv * percentage;
        current_period = speed_to_period(current_speed, pio_speed, full_rotation_steps);
        pio_sm_put(MOTOR_STEPPER_PIO, motor_config->pio_sm, current_period);
    }

    current_period = speed_to_period(new_speed, pio_speed, full_rotation_steps);
    pio_sm_put_blocking(MOTOR_STEPPER_PIO, motor_config->pio_sm, current_period);
}


void stepper_speed_control_task(void * p) {    
    // Currently doing speed control
    while (true) {
        // Wait for new speed
        float new_velocity;
        xQueueReceive(((motor_config_t *) p)->stepper_speed_control_queue, &new_velocity, portMAX_DELAY);

        // Get latest PIO speed, in case of the change of system clock
        uint32_t pio_speed = clock_get_hz(clk_sys);

        // Determine if both have same direction (no need to change DIR pin state)
        if ((new_velocity >= 0) == (((motor_config_t *) p)->prev_velocity >= 0)) {
            // Same direction means only speed change
            speed_ramp(((motor_config_t *) p), 
                       fabs(((motor_config_t *) p)->prev_velocity), 
                       fabs(new_velocity), 
                       pio_speed);
        }
        else {
            // Different direction, then ramp down to 0, change direction then ramp up
            speed_ramp(((motor_config_t *) p), 
                       fabs(((motor_config_t *) p)->prev_velocity),
                       0.0f,
                       pio_speed);
            ((motor_config_t *) p)->step_direction = !((motor_config_t *) p)->step_direction;

            // Toggle the direction
            gpio_put(((motor_config_t *) p)->dir_pin, ((motor_config_t *) p)->step_direction);

            // Ramp to the new speed
            speed_ramp(((motor_config_t *) p), 
                       0.0f,
                       fabs(new_velocity),
                       pio_speed);
        }

        // Update speed
        ((motor_config_t *) p)->prev_velocity = new_velocity;
    }
}   


void motor_task(void *p) {
    bool status = motors_init();

    // Initialize motor related RTOS control
    coarse_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));
    fine_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));


    UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());

    // Create one task for each stepper controller
    xTaskCreate(stepper_speed_control_task, 
                "Coarse Trickler", 
                configMINIMAL_STACK_SIZE, 
                (void *) &coarse_trickler_motor_config, 
                current_task_priority + 1, 
                &coarse_trickler_motor_config.stepper_speed_control_task_handler);

    xTaskCreate(stepper_speed_control_task, 
                "Fine Trickler", 
                configMINIMAL_STACK_SIZE, 
                (void *) &fine_trickler_motor_config, 
                current_task_priority + 1, 
                &fine_trickler_motor_config.stepper_speed_control_task_handler);

    while (true) {
        // Stop here
        vTaskSuspend(NULL);
    }
}


void motor_set_speed(motor_select_t selected_motor, float new_velocity) {
    // Positive speed for positive direction
    motor_config_t * motor_config = NULL;
    switch (selected_motor)
    {
    case SELECT_COARSE_TRICKLER_MOTOR:
        motor_config = &coarse_trickler_motor_config;
        break;
    case SELECT_FINE_TRICKLER_MOTOR:
        motor_config = &fine_trickler_motor_config;
        break;
    
    default:
        break;
    }

    if (motor_config) {
        // Send to the queue only if 
        if (motor_config->stepper_speed_control_queue) {
            xQueueSend(motor_config->stepper_speed_control_queue, &new_velocity, portMAX_DELAY);
        }
    }   
}

void motor_enable(motor_select_t selected_motor, bool enable) {
    motor_config_t * motor_config = NULL;
    switch (selected_motor)
    {
    case SELECT_COARSE_TRICKLER_MOTOR:
        motor_config = &coarse_trickler_motor_config;
        break;
    case SELECT_FINE_TRICKLER_MOTOR:
        motor_config = &fine_trickler_motor_config;
        break;
    
    default:
        break;
    }

    if (motor_config) {
        // Send to the queue only if 
        gpio_put(motor_config->en_pin, !enable);  // off at high

        // If disabled, we shall also disable the stepper signal
        if (!enable) {
            motor_set_speed(selected_motor, 0);
        }
    }   
}