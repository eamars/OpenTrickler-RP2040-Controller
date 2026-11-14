/* Isolate the C and C++ */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include "app.h"
#include "tmc2209.h"
#include "hardware/uart.h"
#include "configuration.h"
#include "hardware/gpio.h"
#include "step_pulses.h"

#include "motors.h"
#include "eeprom.h"
#include "common.h"

// The task that feeds the step pulses, every tick. Above every task that sets a speed: the pulses of a
// motor that should turn must not wait for them
#define MOTOR_STEP_FEED_PRIORITY 10

// A direction is changed with the motor still: this long after the speed is set to zero, so the pulses
// have stopped
#define MOTOR_DIR_CHANGE_DELAY_MS 3

// The drivers' serial line: how long a byte may take to come, the driver's answer delay included
#define MOTOR_UART_BYTE_TIMEOUT_US 2000

/*
    The wire is left idle this long before each frame. A driver that has just answered needs time to listen again:
    a frame sent on the heels of an answer was lost at every start-up (ADR-012's progress, step 1). Klipper leaves
    the same 200 us before every frame (src/tmcuart.c).
*/
#define MOTOR_UART_IDLE_US 200

/*
    `current_ma` was the current only with the driver module's trimmer at full scale; it is now the current itself
    (ADR-012). A figure stored before that is converted once, by the factory trimmer's share of full scale
    (1.2 V of 2.5 V), and rounded to 10 mA.
*/
#define MOTOR_CURRENT_TRIMMER_SHARE 0.48f
#define MOTOR_CURRENT_ROUND_MA 10

/*
    The mark of a converted figure, in the stored struct's `motor_data_rev`. In its upper two bytes: load_config
    takes anything but 0 in the lower two as an EEPROM never written, and stores the defaults.
*/
#define MOTOR_DATA_REV_TRUE_CURRENT 0x00010000u


// Internal data structure for speed control between tasks
typedef struct {
    float new_speed_setpoint;
    float direction;
    float ramp_rate;
} stepper_speed_control_t;


// Configurations
motor_config_t coarse_trickler_motor_config;
motor_config_t fine_trickler_motor_config;


const eeprom_motor_data_t default_motor_data = {
    .motor_data_rev = MOTOR_DATA_REV_TRUE_CURRENT,
    // Motor 0 is coarse trickler
    .motor_data[0] = {
        .full_steps_per_rotation = 200,     // 200: 1.8 deg stepper, 400: 0.9 deg stepper
        .current_ma = 400,                  // mA RMS: what a unit ran at before ADR-012, with 800 stored
        .microsteps = 256,                  // Maximum microstep that TMC2209 supports
        .max_speed_rps = 5,                 // Maximum speed before the stepper starts to lose step
        .r_sense = 110,                     // Default sensing resistor (110ohm is used for stanard TMC2209 modules)

        .angular_acceleration = 50,         // In rev/s^2
        .min_speed_rps = 0.08,              // Minimum speed for powder to drop, can be overridden by the profile
        .gear_ratio = 1.25f,                // 40:32 gear ratio for coarse trickler motor

        .inverted_direction = false,        // Invert the rotation direction if set to true
        .inverted_enable = false,           // Invert the enable flag if set to true
    },
    .motor_data[1] = {
        .full_steps_per_rotation = 200,     // 200: 1.8 deg stepper, 400: 0.9 deg stepper
        .current_ma = 400,                  // mA RMS: what a unit ran at before ADR-012, with 800 stored
        .microsteps = 256,                  // Maximum microstep that TMC2209 supports
        .max_speed_rps = 5,                 // Maximum speed before the stepper starts to lose step
        .r_sense = 110,                     // Default sensing resistor (110ohm is used for stanard TMC2209 modules)

        .angular_acceleration = 50,         // In rev/s^2
        .min_speed_rps = 0.01,              // Minimum speed for powder to drop, can be overridden by the profile
        .gear_ratio = 1.818f,               // Fine trickler gear ratio

        .inverted_direction = false,        // Invert the rotation direction if set to true
        .inverted_enable = false,           // Invert the enable flag if set to true
    },
};


// The two drivers, and what is known of the stored settings
static tmc2209_t coarse_trickler_driver;
static tmc2209_t fine_trickler_driver;
static bool motor_config_loaded = false;
static uint32_t motor_data_rev = MOTOR_DATA_REV_TRUE_CURRENT;

// One task at a time talks to the drivers once the scheduler runs
static SemaphoreHandle_t motor_driver_mutex = NULL;


/*
    One exchange on the drivers' wire, for tmc_uart. The wire is one: TX reaches it through a resistor and RX sits
    on it, so every byte sent comes back first, and is checked. The driver's answer follows.
*/
static bool motor_uart_exchange(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
    bool is_ok = true;

    busy_wait_us(MOTOR_UART_IDLE_US);

    // Whatever is left from before is not this exchange's
    while (uart_is_readable(MOTOR_UART)) {
        uart_getc(MOTOR_UART);
    }

    uart_write_blocking(MOTOR_UART, tx, tx_len);

    for (size_t idx = 0; idx < tx_len; idx += 1) {
        if (!uart_is_readable_within_us(MOTOR_UART, MOTOR_UART_BYTE_TIMEOUT_US)) {
            return false;
        }
        if ((uint8_t) uart_getc(MOTOR_UART) != tx[idx]) {
            is_ok = false;
        }
    }

    for (size_t idx = 0; idx < rx_len; idx += 1) {
        if (!uart_is_readable_within_us(MOTOR_UART, MOTOR_UART_BYTE_TIMEOUT_US)) {
            return false;
        }
        rx[idx] = (uint8_t) uart_getc(MOTOR_UART);
    }

    return is_ok;
}


static void motor_driver_lock(void) {
    if (motor_driver_mutex && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreTake(motor_driver_mutex, portMAX_DELAY);
    }
}


static void motor_driver_unlock(void) {
    if (motor_driver_mutex && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreGive(motor_driver_mutex);
    }
}


static tmc2209_t * motor_get_driver(motor_select_t selected_motor) {
    return selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_driver : &coarse_trickler_driver;
}


// Write a driver its registers from the stored settings. False if it does not answer or does not take them
static bool driver_setup(motor_config_t * motor_config) {
    tmc2209_t * driver = (tmc2209_t *) motor_config->tmc_driver;
    bool was_ready = driver->ready;

    motor_driver_lock();
    bool is_ok = tmc2209_setup(driver,
                               motor_config->persistent_config.current_ma,
                               motor_config->persistent_config.r_sense,
                               motor_config->persistent_config.microsteps);
    motor_driver_unlock();

    // Said when it changes, and each time it fails
    if (is_ok && !was_ready) {
        printf("Motor driver %u: set up, %u mA RMS asked, %u mA given\n",
               motor_config->uart_addr, motor_config->persistent_config.current_ma,
               tmc2209_get_run_current_ma(driver));
    }
    else if (!is_ok) {
        printf("Motor driver %u: no answer, or a register not taken\n", motor_config->uart_addr);
    }

    return is_ok;
}


bool driver_io_init(motor_config_t * motor_config) {
    gpio_init(motor_config->en_pin);
    gpio_set_dir(motor_config->en_pin, GPIO_OUT);

    // If enable is inverted the GPIO shall output enable state directly, otherwise invert it. 
    // If not inverted the GPIO shall be the opposite state of the enable signal
    gpio_put(motor_config->en_pin, motor_config->persistent_config.inverted_enable ? false : true); 

    // Note: STEP PIN is controlled by PIO block
    // gpio_init(motor_config->step_pin);
    // gpio_set_dir(motor_config->step_pin, GPIO_OUT);
    // gpio_put(motor_config->step_pin, 1);

    gpio_init(motor_config->dir_pin);
    gpio_set_dir(motor_config->dir_pin, GPIO_OUT);
    gpio_put(motor_config->dir_pin, motor_config->step_direction);

    return true;
}

bool motor_config_init(void) {
    bool is_ok = true;

    memset(&coarse_trickler_motor_config, 0x0, sizeof(motor_config_t));
    memset(&fine_trickler_motor_config, 0x0, sizeof(motor_config_t));

    // Read motor config from EEPROM
    eeprom_motor_data_t eeprom_motor_data;
    memset(&eeprom_motor_data, 0x0, sizeof(eeprom_motor_data));
    is_ok = load_config(EEPROM_MOTOR_CONFIG_BASE_ADDR, &eeprom_motor_data, &default_motor_data, sizeof(eeprom_motor_data), EEPROM_MOTOR_DATA_REV);
    if (!is_ok) {
        printf("Unable to read motor configuration\n");
        return is_ok;
    }
    
    // Copy the initialized data back to the stack
    memcpy(&coarse_trickler_motor_config.persistent_config, &eeprom_motor_data.motor_data[0], sizeof(motor_persistent_config_t));
    memcpy(&fine_trickler_motor_config.persistent_config, &eeprom_motor_data.motor_data[1], sizeof(motor_persistent_config_t));
    motor_data_rev = eeprom_motor_data.motor_data_rev;

    // Figures stored before ADR-012 are converted once: the motors keep the current they ran at
    if (motor_data_rev != MOTOR_DATA_REV_TRUE_CURRENT) {
        motor_config_t * motor_configs[] = {&coarse_trickler_motor_config, &fine_trickler_motor_config};
        for (int idx = 0; idx < 2; idx += 1) {
            uint16_t stored_ma = motor_configs[idx]->persistent_config.current_ma;
            uint16_t true_ma = (uint16_t) (lroundf(stored_ma * MOTOR_CURRENT_TRIMMER_SHARE / MOTOR_CURRENT_ROUND_MA) *
                                           MOTOR_CURRENT_ROUND_MA);
            motor_configs[idx]->persistent_config.current_ma = true_ma;
            printf("Motor %d: stored current %u mA converted to %u mA RMS\n", idx, stored_ma, true_ma);
        }

        motor_data_rev = MOTOR_DATA_REV_TRUE_CURRENT;
        if (!motor_config_save()) {
            printf("Unable to store the converted motor current\n");
        }
    }

    // Set initial direction
    coarse_trickler_motor_config.step_direction = coarse_trickler_motor_config.persistent_config.inverted_direction ? true : false;
    fine_trickler_motor_config.step_direction = fine_trickler_motor_config.persistent_config.inverted_direction ? true : false;

    // Register to eeprom save all
    eeprom_register_handler(motor_config_save);

    return is_ok;
}


bool motor_config_save() {
    bool is_ok;
    eeprom_motor_data_t eeprom_motor_data;

    // Not a revision any more: its lower two bytes stay 0 for load_config, its upper two mark the true current
    eeprom_motor_data.motor_data_rev = motor_data_rev;

    // Copy the live data to the EEPROM structure
    memcpy(&eeprom_motor_data.motor_data[0], &coarse_trickler_motor_config.persistent_config, sizeof(motor_persistent_config_t));
    memcpy(&eeprom_motor_data.motor_data[1], &fine_trickler_motor_config.persistent_config, sizeof(motor_persistent_config_t));

    is_ok = save_config(EEPROM_MOTOR_CONFIG_BASE_ADDR, &eeprom_motor_data, sizeof(eeprom_motor_data_t));

    return is_ok;
}


void speed_ramp(motor_config_t * motor_config, float prev_speed, float new_speed) {
    // Calculate ramp param
    float dv = new_speed - prev_speed;
    float ramp_time_s = fabs(dv / motor_config->persistent_config.angular_acceleration);
    uint32_t full_rotation_steps = motor_config->persistent_config.full_steps_per_rotation * motor_config->persistent_config.microsteps;

    // Calculate termination condition
    uint32_t ramp_time_us = (uint32_t) (fabs(ramp_time_s) * 1e6);
    uint32_t start_time = time_us_32();
    uint32_t stop_time = start_time + ramp_time_us;

    float current_speed;
    while (true) {
        uint32_t current_time = time_us_32();
        if (current_time > stop_time) {
            break;
        }

        float percentage = (current_time - start_time) / (float) ramp_time_us;

        current_speed = prev_speed + dv * percentage;
        step_pulses_set_rate(motor_config->pulse_channel, current_speed * full_rotation_steps);
    }

    step_pulses_set_rate(motor_config->pulse_channel, new_speed * full_rotation_steps);
}


void stepper_speed_control_task(void * p) {    
    // Currently doing speed control
    while (true) {
        // Wait for new speed
        float new_velocity;
        xQueueReceive(((motor_config_t *) p)->stepper_speed_control_queue, &new_velocity, portMAX_DELAY);

        // Calculate the speed of the motor
        new_velocity /= ((motor_config_t *) p)->persistent_config.gear_ratio;

        // Determine if both have same direction (no need to change DIR pin state)
        if ((new_velocity >= 0) == (((motor_config_t *) p)->prev_velocity >= 0)) {
            // Same direction means only speed change
            speed_ramp(((motor_config_t *) p), 
                       fabs(((motor_config_t *) p)->prev_velocity), 
                       fabs(new_velocity));
        }
        else {
            // Different direction, then ramp down to 0, change direction then ramp up
            speed_ramp(((motor_config_t *) p), 
                       fabs(((motor_config_t *) p)->prev_velocity),
                       0.0f);
            // The pulses stop at the next feed: the direction changes on a motor that is still
            vTaskDelay(pdMS_TO_TICKS(MOTOR_DIR_CHANGE_DELAY_MS));
            ((motor_config_t *) p)->step_direction = !((motor_config_t *) p)->step_direction;

            // Toggle the direction
            gpio_put(((motor_config_t *) p)->dir_pin, ((motor_config_t *) p)->step_direction);

            // Ramp to the new speed
            speed_ramp(((motor_config_t *) p), 
                       0.0f,
                       fabs(new_velocity));
        }

        // Update speed
        ((motor_config_t *) p)->prev_velocity = new_velocity;
    }
}   


/*
    Feeds the step pulses, every tick. The PIO makes only the steps it is fed (ADR-012): if this task stops,
    whatever stopped it, the motors stop within a few milliseconds.
*/
static void motor_step_feed_task(void * p) {
    TickType_t last_wake_tick = xTaskGetTickCount();

    while (true) {
        step_pulses_feed();
        vTaskDelayUntil(&last_wake_tick, 1);
    }
}


void motor_set_speed(motor_select_t selected_motor, float new_velocity) {
    if (selected_motor == SELECT_COARSE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        if (coarse_trickler_motor_config.stepper_speed_control_queue) {
            xQueueSend(coarse_trickler_motor_config.stepper_speed_control_queue, &new_velocity, portMAX_DELAY);
        }
    }

    if (selected_motor == SELECT_FINE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        if (fine_trickler_motor_config.stepper_speed_control_queue) {
            xQueueSend(fine_trickler_motor_config.stepper_speed_control_queue, &new_velocity, portMAX_DELAY);
        }
    }
}


float motor_get_velocity(motor_select_t selected_motor) {
    // prev_velocity is the motor's own speed, set once a ramp ends: back to the trickler's, as motor_set_speed takes it
    motor_config_t *motor_config = selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_motor_config
                                                                                : &coarse_trickler_motor_config;
    return motor_config->prev_velocity * motor_config->persistent_config.gear_ratio;
}


void motor_enable(motor_select_t selected_motor, bool enable) {
    if (selected_motor == SELECT_COARSE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        bool en_signal = coarse_trickler_motor_config.persistent_config.inverted_enable ? enable : !enable;

        gpio_put(coarse_trickler_motor_config.en_pin, en_signal);

        // If disabled, we shall also disable the stepper signal
        if (!enable) {
            motor_set_speed(selected_motor, 0);
        }
    }

    if (selected_motor == SELECT_FINE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        bool en_signal = fine_trickler_motor_config.persistent_config.inverted_enable ? enable : !enable;

        gpio_put(fine_trickler_motor_config.en_pin, en_signal);

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
        assert(false);
        break;
    }

    if (motor_config) {
        return motor_config->persistent_config.max_speed_rps;
    }

    return 0;
}


float get_motor_min_speed(motor_select_t selected_motor) {
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
        return motor_config->persistent_config.min_speed_rps;
    }

    return 0.0f;
}


void motors_init(void) {
    // The stored settings. If they cannot be read the defaults stand in, so the pins and tasks below are still
    // set up, and motors_get_fault() says so: the modes do not start
    motor_config_loaded = motor_config_init();
    if (!motor_config_loaded) {
        memcpy(&coarse_trickler_motor_config.persistent_config, &default_motor_data.motor_data[0], sizeof(motor_persistent_config_t));
        memcpy(&fine_trickler_motor_config.persistent_config, &default_motor_data.motor_data[1], sizeof(motor_persistent_config_t));
    }

    coarse_trickler_motor_config.dir_pin = COARSE_MOTOR_DIR_PIN;
    coarse_trickler_motor_config.en_pin = COARSE_MOTOR_EN_PIN;
    coarse_trickler_motor_config.step_pin = COARSE_MOTOR_STEP_PIN;
    coarse_trickler_motor_config.uart_addr = COARSE_MOTOR_ADDR;
    coarse_trickler_motor_config.tmc_driver = &coarse_trickler_driver;
    coarse_trickler_motor_config.pulse_channel = 0;

    fine_trickler_motor_config.dir_pin = FINE_MOTOR_DIR_PIN;
    fine_trickler_motor_config.en_pin = FINE_MOTOR_EN_PIN;
    fine_trickler_motor_config.step_pin = FINE_MOTOR_STEP_PIN;
    fine_trickler_motor_config.uart_addr = FINE_MOTOR_ADDR;
    fine_trickler_motor_config.tmc_driver = &fine_trickler_driver;
    fine_trickler_motor_config.pulse_channel = 1;

    // TMC driver doesn't care about the baud rate the host is using
    uart_init(MOTOR_UART, 250000);
    gpio_set_function(MOTOR_UART_RX, GPIO_FUNC_UART);
    gpio_set_function(MOTOR_UART_TX, GPIO_FUNC_UART);

    motor_driver_mutex = xSemaphoreCreateMutex();
    if (!motor_driver_mutex) {
        printf("Unable to create the motor drivers' mutex\n");
    }

    tmc2209_init(&coarse_trickler_driver, motor_uart_exchange, COARSE_MOTOR_ADDR);
    tmc2209_init(&fine_trickler_driver, motor_uart_exchange, FINE_MOTOR_ADDR);

    // Pins and step pulses of both motors
    driver_io_init(&coarse_trickler_motor_config);
    driver_io_init(&fine_trickler_motor_config);

    if (!step_pulses_init(coarse_trickler_motor_config.pulse_channel, coarse_trickler_motor_config.step_pin) ||
        !step_pulses_init(fine_trickler_motor_config.pulse_channel, fine_trickler_motor_config.step_pin)) {
        printf("Unable to set up the motors' step pulses\n");
    }

    // The drivers. One that does not answer is not a halt (ADR-012): the unit starts, motors_get_fault() names it,
    // and motors_ready() tries it again each time a mode is about to turn a motor
    driver_setup(&coarse_trickler_motor_config);
    driver_setup(&fine_trickler_motor_config);

    // Initialize motor related RTOS control
    coarse_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));
    fine_trickler_motor_config.stepper_speed_control_queue = xQueueCreate(2, sizeof(stepper_speed_control_t));

    xTaskCreate(motor_step_feed_task, "Step Feed", configMINIMAL_STACK_SIZE, NULL, MOTOR_STEP_FEED_PRIORITY, NULL);

    // Create one task for each stepper controller
    xTaskCreate(stepper_speed_control_task,
                "Coarse Trickler",
                configMINIMAL_STACK_SIZE,
                (void *) &coarse_trickler_motor_config,
                9,  // Coarse trickler at higher priority to response faster to stop
                &coarse_trickler_motor_config.stepper_speed_control_task_handler);

    xTaskCreate(stepper_speed_control_task,
                "Fine Trickler",
                configMINIMAL_STACK_SIZE,
                (void *) &fine_trickler_motor_config,
                8,
                &fine_trickler_motor_config.stepper_speed_control_task_handler);
}


void motors_fault_stop(void) {
    // Register writes only: the pulses off, then the drivers
    step_pulses_fault_stop();
    gpio_put(coarse_trickler_motor_config.en_pin, coarse_trickler_motor_config.persistent_config.inverted_enable ? false : true);
    gpio_put(fine_trickler_motor_config.en_pin, fine_trickler_motor_config.persistent_config.inverted_enable ? false : true);
}


void motor_get_step_stats(motor_select_t selected_motor, step_pulses_stats_t * stats) {
    motor_config_t * motor_config = selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_motor_config
                                                                                 : &coarse_trickler_motor_config;
    step_pulses_get_stats(motor_config->pulse_channel, stats);
}


float motor_get_step_rate(motor_select_t selected_motor) {
    motor_config_t * motor_config = selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_motor_config
                                                                                 : &coarse_trickler_motor_config;
    return step_pulses_get_rate(motor_config->pulse_channel);
}


const char * motors_get_fault(void) {
    if (!motor_config_loaded) {
        return "No motor settings";
    }
    if (!coarse_trickler_driver.ready) {
        return "No coarse driver";
    }
    if (!fine_trickler_driver.ready) {
        return "No fine driver";
    }
    return NULL;
}


bool motors_ready(void) {
    // A driver that did not answer at start-up may by now: its supply can come after the Pico's
    if (!coarse_trickler_driver.ready) {
        driver_setup(&coarse_trickler_motor_config);
    }
    if (!fine_trickler_driver.ready) {
        driver_setup(&fine_trickler_motor_config);
    }

    return motors_get_fault() == NULL;
}


bool motor_driver_setup(motor_select_t selected_motor) {
    return driver_setup(selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_motor_config
                                                                     : &coarse_trickler_motor_config);
}


void motor_driver_set_lost(motor_select_t selected_motor, bool lost) {
    tmc2209_t * driver = motor_get_driver(selected_motor);
    uint8_t addr = selected_motor == SELECT_FINE_TRICKLER_MOTOR ? FINE_MOTOR_ADDR : COARSE_MOTOR_ADDR;

    // Address 3 is nobody's on this board: asked there, no driver answers
    motor_driver_lock();
    driver->bus.addr = lost ? 3 : addr;
    driver->bus.ifcnt_known = false;
    driver->ready = false;
    motor_driver_unlock();
}


bool motor_driver_dump(motor_select_t selected_motor, motor_driver_line_fn emit, void * ctx) {
    motor_driver_lock();
    bool is_ok = tmc2209_dump(motor_get_driver(selected_motor), emit, ctx);
    motor_driver_unlock();

    return is_ok;
}


void motor_driver_get_stats(motor_select_t selected_motor, motor_driver_stats_t * stats) {
    tmc2209_t * driver = motor_get_driver(selected_motor);

    stats->ready = driver->ready;
    stats->current_ma = tmc2209_get_run_current_ma(driver);
    stats->writes = driver->bus.writes;
    stats->write_retries = driver->bus.write_retries;
    stats->read_retries = driver->bus.read_retries;
    stats->failures = driver->bus.failures;
}


const char * get_motor_select_string(motor_select_t selected_motor) {
    if (selected_motor == SELECT_COARSE_TRICKLER_MOTOR) {
        return "Coarse";
    }
    else if (selected_motor == SELECT_FINE_TRICKLER_MOTOR) {
        return "Fine";
    }
    else if (selected_motor == SELECT_BOTH_MOTOR) {
        return "Both";
    }

    assert(false);

    return NULL;
}


void populate_rest_motor_config(motor_config_t * motor_config, char * buf, size_t max_len) {
    // Mappings:
    // m0 (float): angular_acceleration
    // m1 (int): full_steps_per_rotation
    // m2 (int): current_ma
    // m3 (int): microsteps
    // m4 (int): max_speed_rps
    // m5 (int): r_sense
    // m6 (float): min_speed_rps
    // m7 (float): gear_ratio
    // m8 (bool): inverted_enable
    // m9 (bool): inverted_direction
    // ee (bool): save to eeprom

    // Build response
    snprintf(buf, 
             max_len,
             "%s"
             "{\"m0\":%0.3f,\"m1\":%ld,\"m2\":%d,\"m3\":%d,\"m4\":%d,\"m5\":%d,\"m6\":%0.3f,\"m7\":%0.7f,\"m8\":%s,\"m9\":%s}",
             http_json_header,
             motor_config->persistent_config.angular_acceleration, 
             motor_config->persistent_config.full_steps_per_rotation,
             motor_config->persistent_config.current_ma,
             motor_config->persistent_config.microsteps,
             motor_config->persistent_config.max_speed_rps,
             motor_config->persistent_config.r_sense,
             motor_config->persistent_config.min_speed_rps,
             motor_config->persistent_config.gear_ratio,
             boolean_to_string(motor_config->persistent_config.inverted_enable),
             boolean_to_string(motor_config->persistent_config.inverted_direction));
}

void apply_rest_motor_config(motor_config_t * motor_config, int num_params, char *params[], char *values[]) {
    bool save_to_eeprom = false;

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "m0") == 0) {
            float angular_acceleration = strtof(values[idx], NULL);
            motor_config->persistent_config.angular_acceleration = angular_acceleration;
        }
        else if (strcmp(params[idx], "m1") == 0) {
            uint32_t full_steps_per_rotation = strtol(values[idx], NULL, 10);
            motor_config->persistent_config.full_steps_per_rotation = full_steps_per_rotation;
        }
        else if (strcmp(params[idx], "m2") == 0) {
            uint16_t current_ma = (uint16_t) atoi(values[idx]);
            motor_config->persistent_config.current_ma = current_ma;
        }
        else if (strcmp(params[idx], "m3") == 0) {
            uint16_t microsteps = (uint16_t) atoi(values[idx]);
            motor_config->persistent_config.microsteps = microsteps;
        }
        else if (strcmp(params[idx], "m4") == 0) {
            uint16_t max_speed_rps = (uint16_t) atoi(values[idx]);
            motor_config->persistent_config.max_speed_rps = max_speed_rps;
        }
        else if (strcmp(params[idx], "m5") == 0) {
            uint16_t r_sense = (uint16_t) atoi(values[idx]);
            motor_config->persistent_config.r_sense = r_sense;
        }
        else if (strcmp(params[idx], "m6") == 0) {
            float min_speed_rps = strtof(values[idx], NULL);
            motor_config->persistent_config.min_speed_rps = min_speed_rps;
        }
        else if (strcmp(params[idx], "m7") == 0) {
            float gear_ratio = strtof(values[idx], NULL);
            motor_config->persistent_config.gear_ratio = gear_ratio;
        }
        else if (strcmp(params[idx], "m8") == 0) {
            bool inverted_enable = string_to_boolean(values[idx]);
            motor_config->persistent_config.inverted_enable = inverted_enable;
        }
        else if (strcmp(params[idx], "m9") == 0) {
            bool inverted_direction = string_to_boolean(values[idx]);
            motor_config->persistent_config.inverted_direction = inverted_direction;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        motor_config_save();  // Note: this will save settings for both
    }
}


bool http_rest_coarse_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char json_buffer[256];

    apply_rest_motor_config(&coarse_trickler_motor_config, num_params, params, values);
    populate_rest_motor_config(&coarse_trickler_motor_config, json_buffer, sizeof(json_buffer));

    size_t response_len = strlen(json_buffer);
    file->data = json_buffer;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_fine_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char json_buffer[256];

    apply_rest_motor_config(&fine_trickler_motor_config, num_params, params, values);
    populate_rest_motor_config(&fine_trickler_motor_config, json_buffer, sizeof(json_buffer));

    size_t response_len = strlen(json_buffer);
    file->data = json_buffer;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
