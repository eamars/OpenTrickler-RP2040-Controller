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
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "stepper.pio.h"

#include "motors.h"
#include "eeprom.h"


// Internal data structure for speed control between tasks
typedef struct {
    float new_speed_setpoint;
    float direction;
    float ramp_rate;
} stepper_speed_control_t;


// Configurations
motor_config_t coarse_trickler_motor_config;
motor_config_t fine_trickler_motor_config;


const motor_persistent_config_t default_motor_persistent_config = {
    .angular_acceleration = 2000,       // 2000 rev/s^2
    .current_ma = 500,                  // 500 mA
    .full_steps_per_rotation = 200,     // 200: 1.8 deg stepper, 400: 0.9 deg stepper
    .max_speed_rps = 20,                // Maximum speed before the stepper runs out
    .microsteps = 256,                  // Default to maximum that the driver supports
    .r_sense = 110,                     // 0.110 ohm sense resistor the stepper driver
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


void swuart_calcCRC(uint8_t* datagram, uint8_t datagramLength)
{
    int i,j;
    uint8_t* crc = datagram + (datagramLength-1); // CRC located in last byte of message
    uint8_t currentByte;
    *crc = 0;
    for (i=0; i<(datagramLength-1); i++) { // Execute for all bytes of a message
        currentByte = datagram[i]; // Retrieve a byte to be sent from Array
        for (j=0; j<8; j++) {
            if ((*crc >> 7) ^ (currentByte&0x01)) // update CRC based result of XOR operation
            {
                *crc = (*crc << 1) ^ 0x07;
            }
            else
            {
                *crc = (*crc << 1);
            }
            currentByte = currentByte >> 1;
        } // for CRC bit
    } // for message byte
}



void tmc_uart_write (trinamic_motor_t driver, TMC_uart_write_datagram_t *datagram)
{
    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_write_datagram_t));
}

TMC_uart_write_datagram_t *tmc_uart_read (trinamic_motor_t driver, TMC_uart_read_datagram_t *datagram)
{
    static TMC_uart_write_datagram_t wdgr = {0}; 

    uart_write_blocking(MOTOR_UART, datagram->data, sizeof(TMC_uart_read_datagram_t));

    // At 250k baud rate the transmit time is about 320us. Need to wait long enough to not reading the echo message back.
    busy_wait_us(20);
    _enable_uart_rx(MOTOR_UART, true);
    busy_wait_ms(2);
    
    uint8_t sync_flag = 0x05;
    int8_t idx = -1;
    while (uart_is_readable_within_us(MOTOR_UART, 2000)) {
        uint8_t c;

        uart_read_blocking(MOTOR_UART, &c, 1);
        if (c == sync_flag) {
            idx = 0;
        }

        if (idx >= 0 && idx < 8) {
            wdgr.data[idx++] = c;
        }

        if (idx == 8) {
            // FIXME: there is known issue that calling IFCNT causes target addr to be incorrect. 
            // Calculate CRC
            uint8_t crc = wdgr.msg.crc;
            swuart_calcCRC(wdgr.data, sizeof(TMC_uart_write_datagram_t));
            if (crc == wdgr.msg.crc) {
                break;
            }
            idx = -1;
        }
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
    if (delay_period >= 4) {
        delay_period -= 4;
    }
    else {
        delay_period = 0;
    }

    return delay_period;
}


bool tmc2209_init (TMC2209_t *driver)
{
    // Perform a status register read/write to clear status flags.
    // If no or bad response from driver return with error.
    if(!TMC2209_ReadRegister(driver, (TMC2209_datagram_t *)&driver->gstat))
        return false;

    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->gstat);

    TMC2209_ReadRegister(driver, (TMC2209_datagram_t *)&driver->gconf);
    driver->gconf.reg.pdn_disable = 1;
    driver->gconf.reg.mstep_reg_select = 1;

// Use default settings (from OTP) for these:
//  driver->gconf.reg.I_scale_analog = 1;
//  driver->gconf.reg.internal_Rsense = 0;
//  driver->gconf.reg.en_spreadcycle = 0;
//  driver->gconf.reg.multistep_filt = 1;

    TMC2209_ReadRegister(driver, (TMC2209_datagram_t *)&driver->ifcnt);

    uint8_t ifcnt = driver->ifcnt.reg.count;

    driver->chopconf.reg.mres = tmc_microsteps_to_mres(driver->config.microsteps);

    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->gconf);
    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->tpowerdown);
    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->pwmconf);
    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->tpwmthrs);
    TMC2209_WriteRegister(driver, (TMC2209_datagram_t *)&driver->tcoolthrs);
    TMC2209_SetCurrent(driver, driver->config.current, driver->config.hold_current_pct);

    int retry = 5;
    while (retry-- > 0) {
        TMC2209_ReadRegister(driver, (TMC2209_datagram_t *)&driver->ifcnt);
        uint8_t new_cnt = (uint8_t)driver->ifcnt.reg.count;

        if (new_cnt - ifcnt == 7) {
            return true;
        }
    }
    

    return false;
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
    tmc_driver->config.current = motor_config->persistent_config.current_ma;
    tmc_driver->config.r_sense = motor_config->persistent_config.r_sense;
    tmc_driver->config.hold_current_pct = 50;
    tmc_driver->config.microsteps = motor_config->persistent_config.microsteps;

    bool is_ok = tmc2209_init(tmc_driver);

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


bool motor_config_init(void) {
    bool is_ok = true;

    // Read motor config from EEPROM
    eeprom_motor_data_t eeprom_motor_data;
    is_ok = eeprom_read(EEPROM_MOTOR_CONFIG_BASE_ADDR, (uint8_t *)&eeprom_motor_data, sizeof(eeprom_motor_data_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_MOTOR_CONFIG_BASE_ADDR);
        return false;
    }

    // If the revision doesn't match then re-initialize the config
    if (eeprom_motor_data.motor_data_rev != EEPROM_MOTOR_DATA_REV) {
        memcpy(&eeprom_motor_data.motor_data[0], &default_motor_persistent_config, sizeof(motor_persistent_config_t));
        memcpy(&eeprom_motor_data.motor_data[1], &default_motor_persistent_config, sizeof(motor_persistent_config_t));
        eeprom_motor_data.motor_data_rev = EEPROM_MOTOR_DATA_REV;

        // Write data back
        is_ok = eeprom_write(EEPROM_MOTOR_CONFIG_BASE_ADDR, (uint8_t *) &eeprom_motor_data, sizeof(eeprom_motor_data_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_MOTOR_CONFIG_BASE_ADDR);
            return false;
        }
    }
    
    // Copy the initialized data back to the stack
    memcpy(&coarse_trickler_motor_config.persistent_config, &eeprom_motor_data.motor_data[0], sizeof(motor_persistent_config_t));
    memcpy(&fine_trickler_motor_config.persistent_config, &eeprom_motor_data.motor_data[1], sizeof(motor_persistent_config_t));

    return is_ok;
}


bool motor_config_save() {
    bool is_ok;
    eeprom_motor_data_t eeprom_motor_data;

    // Copy the live data to the EEPROM structure
    memcpy(&eeprom_motor_data.motor_data[0], &coarse_trickler_motor_config.persistent_config, sizeof(motor_persistent_config_t));
    memcpy(&eeprom_motor_data.motor_data[1], &fine_trickler_motor_config.persistent_config, sizeof(motor_persistent_config_t));

    is_ok = eeprom_write(EEPROM_MOTOR_CONFIG_BASE_ADDR, (uint8_t *) &eeprom_motor_data, sizeof(eeprom_motor_data_t));

    return is_ok;
}


void speed_ramp(motor_config_t * motor_config, float prev_speed, float new_speed, uint32_t pio_speed) {
    // Calculate ramp param
    float dv = new_speed - prev_speed;
    float ramp_time_s = fabs(dv / motor_config->persistent_config.angular_acceleration);
    uint32_t full_rotation_steps = motor_config->persistent_config.full_steps_per_rotation * motor_config->persistent_config.microsteps;

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


uint16_t get_motor_max_speed(motor_select_t selected_motor) {
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
        return motor_config->persistent_config.max_speed_rps;
    }

    return 0;
}


bool motors_init(void) {
    bool is_ok;

    // Initialize config
    is_ok = motor_config_init();
    assert(is_ok);

    // Assume the `motor_config_init` is already called
    coarse_trickler_motor_config.dir_pin = COARSE_MOTOR_DIR_PIN;
    coarse_trickler_motor_config.en_pin = COARSE_MOTOR_EN_PIN;
    coarse_trickler_motor_config.step_pin = COARSE_MOTOR_STEP_PIN;
    coarse_trickler_motor_config.uart_addr = COARSE_MOTOR_ADDR;

    fine_trickler_motor_config.dir_pin = FINE_MOTOR_DIR_PIN;
    fine_trickler_motor_config.en_pin = FINE_MOTOR_EN_PIN;
    fine_trickler_motor_config.step_pin = FINE_MOTOR_STEP_PIN;
    fine_trickler_motor_config.uart_addr = FINE_MOTOR_ADDR;

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 250000);
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
    assert(is_ok);

    // 
    // Initialize fine trickler motor at UART ADDR 1
    // 
    driver_io_init(&fine_trickler_motor_config);

    // Allocate PIO to the stepper
    driver_pio_init(&fine_trickler_motor_config);
    
    // Initialize the stepper driver
    is_ok = driver_init(&fine_trickler_motor_config);
    assert(is_ok);

    // Initialize motor related RTOS control
    coarse_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));
    fine_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));

    UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());

    // Create one task for each stepper controller
    xTaskCreate(stepper_speed_control_task, 
                "Coarse Trickler", 
                configMINIMAL_STACK_SIZE, 
                (void *) &coarse_trickler_motor_config, 
                8, 
                &coarse_trickler_motor_config.stepper_speed_control_task_handler);

    xTaskCreate(stepper_speed_control_task, 
                "Fine Trickler", 
                configMINIMAL_STACK_SIZE, 
                (void *) &fine_trickler_motor_config, 
                8, 
                &fine_trickler_motor_config.stepper_speed_control_task_handler);

    return true;
}


char * motor_config_to_json(motor_select_t selected_motor) {
    static char motor_config_json_buffer[256];
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

    if (!motor_config) {
        exit(-1);
    }

    sprintf(motor_config_json_buffer, 
            "{\"angular_acceleration\":%f,\"full_steps_per_rotation\":%d,\"current_ma\":%d,\"microsteps\":%d,\"max_speed_rps\":%d,\"r_sense\":%d}",
            motor_config->persistent_config.angular_acceleration, 
            motor_config->persistent_config.full_steps_per_rotation,
            motor_config->persistent_config.current_ma,
            motor_config->persistent_config.microsteps,
            motor_config->persistent_config.max_speed_rps,
            motor_config->persistent_config.r_sense);
    return motor_config_json_buffer;
}
