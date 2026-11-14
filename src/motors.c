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

// The motors' task: every tick it moves each speed toward its target and feeds the step pulses. Above every task
// that sets a speed: the pulses of a motor that should turn must not wait for them
#define MOTOR_TASK_PRIORITY 10

// What a stored or a given setting may be. Outside these it is refused: several are divided by (ADR-012, fault 6)
#define MOTOR_MIN_ACCELERATION 0.1f         // rev/s^2
#define MOTOR_MAX_ACCELERATION 10000.0f
#define MOTOR_MIN_FULL_STEPS 20             // a rotation
#define MOTOR_MAX_FULL_STEPS 2000
#define MOTOR_MIN_CURRENT_MA 10             // RMS
#define MOTOR_MAX_CURRENT_MA 2000
#define MOTOR_MIN_MAX_SPEED_RPS 1
#define MOTOR_MAX_MAX_SPEED_RPS 100
#define MOTOR_MIN_R_SENSE 50                // milliohm
#define MOTOR_MAX_R_SENSE 1000
#define MOTOR_MIN_GEAR_RATIO 0.01f
#define MOTOR_MAX_GEAR_RATIO 100.0f

// The drivers are read this often while the unit runs, by a task below every other of the firmware's own
#define MOTOR_STATUS_PERIOD_MS 1000
#define MOTOR_STATUS_TASK_PRIORITY 2

// A fault flag counts when it is read this many times running, and a driver is gone after this many readings with
// no answer: Klipper's figures (klippy/extras/tmc.py)
#define MOTOR_STATUS_FAULT_READS 3
#define MOTOR_STATUS_MISSED_POLLS 3

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

// What a driver was last found to be. A fault stays until a mode asks motors_ready() again: the charge it came in
// must see it, even when the driver is well again a second later
typedef enum {
    MOTOR_DRIVER_OK = 0,
    MOTOR_DRIVER_ABSENT,        // It does not answer
    MOTOR_DRIVER_SHORT,         // It reports a coil shorted to ground or to the supply
    MOTOR_DRIVER_HOT,           // It reports it switched off for heat
    MOTOR_DRIVER_RESET,         // It started anew: it lost its supply, and its registers with it
    MOTOR_DRIVER_SUPPLY,        // It reports its charge pump's voltage fell
    MOTOR_DRIVER_FAULT_COUNT,
} motor_driver_fault_t;

static const char * const motor_driver_fault_words[MOTOR_DRIVER_FAULT_COUNT] = {
    "ok", "absent", "short", "hot", "reset", "supply",
};

// For the screen's title line: the motor, then the fault, in few enough letters to fit it
static const char * const motor_driver_fault_titles[2][MOTOR_DRIVER_FAULT_COUNT] = {
    {NULL, "No coarse driver", "Coarse driver short", "Coarse driver hot", "Coarse driver reset", "Coarse driver supply"},
    {NULL, "No fine driver", "Fine driver short", "Fine driver hot", "Fine driver reset", "Fine driver supply"},
};

// By motor: 0 the coarse trickler's, 1 the fine's
static motor_driver_fault_t motor_driver_faults[2] = {MOTOR_DRIVER_ABSENT, MOTOR_DRIVER_ABSENT};
static uint32_t motor_driver_last_drv_status[2];
static uint8_t motor_driver_missed_polls[2];

// For proofs, never stored: flags put into what a driver is read to report
static uint32_t motor_driver_injected_drv_status[2];
static uint32_t motor_driver_injected_gstat[2];

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


static motor_config_t * motor_get_config(motor_select_t selected_motor) {
    return selected_motor == SELECT_FINE_TRICKLER_MOTOR ? &fine_trickler_motor_config : &coarse_trickler_motor_config;
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

    // A driver that answers is no longer absent; any other fault stays for the mode that must see it
    if (!is_ok) {
        motor_driver_faults[motor_config->pulse_channel] = MOTOR_DRIVER_ABSENT;
    }
    else if (motor_driver_faults[motor_config->pulse_channel] == MOTOR_DRIVER_ABSENT) {
        motor_driver_faults[motor_config->pulse_channel] = MOTOR_DRIVER_OK;
    }

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

static bool motor_persistent_config_is_valid(const motor_persistent_config_t * config) {
    return isfinite(config->angular_acceleration) &&
           config->angular_acceleration >= MOTOR_MIN_ACCELERATION && config->angular_acceleration <= MOTOR_MAX_ACCELERATION &&
           config->full_steps_per_rotation >= MOTOR_MIN_FULL_STEPS && config->full_steps_per_rotation <= MOTOR_MAX_FULL_STEPS &&
           config->current_ma >= MOTOR_MIN_CURRENT_MA && config->current_ma <= MOTOR_MAX_CURRENT_MA &&
           tmc2209_microsteps_is_valid(config->microsteps) &&
           config->max_speed_rps >= MOTOR_MIN_MAX_SPEED_RPS && config->max_speed_rps <= MOTOR_MAX_MAX_SPEED_RPS &&
           config->r_sense >= MOTOR_MIN_R_SENSE && config->r_sense <= MOTOR_MAX_R_SENSE &&
           isfinite(config->min_speed_rps) &&
           config->min_speed_rps >= 0.0f && config->min_speed_rps <= config->max_speed_rps &&
           isfinite(config->gear_ratio) &&
           config->gear_ratio >= MOTOR_MIN_GEAR_RATIO && config->gear_ratio <= MOTOR_MAX_GEAR_RATIO;
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

    // A stored setting out of range would be divided by, or drive the motor wrong: that motor gets the defaults,
    // in RAM only
    motor_config_t * loaded_configs[] = {&coarse_trickler_motor_config, &fine_trickler_motor_config};
    for (int idx = 0; idx < 2; idx += 1) {
        if (!motor_persistent_config_is_valid(&loaded_configs[idx]->persistent_config)) {
            printf("Motor %d: a stored setting is out of range, the defaults are used\n", idx);
            memcpy(&loaded_configs[idx]->persistent_config, &default_motor_data.motor_data[idx], sizeof(motor_persistent_config_t));
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


/*
    One tick of one motor. Its speed moves toward its target by the acceleration, and the step pulses are given the
    new rate. A change of direction goes through a stop: the speed is held at zero for the tick in which the pulses
    end, and the direction pin changes in the next, on a motor that is still.
*/
static void motor_tick(motor_config_t * motor_config, float dt_s) {
    float target = motor_config->target_velocity;
    float velocity = motor_config->velocity;
    float max_change = motor_config->persistent_config.angular_acceleration * dt_s;

    if (target > velocity + max_change) {
        velocity += max_change;
    }
    else if (target < velocity - max_change) {
        velocity -= max_change;
    }
    else {
        velocity = target;
    }

    if (velocity != 0.0f) {
        bool wanted_direction = (velocity < 0.0f) != motor_config->persistent_config.inverted_direction;
        if (wanted_direction != motor_config->step_direction) {
            if (motor_config->velocity != 0.0f) {
                velocity = 0.0f;
            }
            else {
                motor_config->step_direction = wanted_direction;
                gpio_put(motor_config->dir_pin, wanted_direction);
            }
        }
    }

    motor_config->velocity = velocity;
    step_pulses_set_rate(motor_config->pulse_channel,
                         fabsf(velocity) * motor_config->persistent_config.full_steps_per_rotation *
                         motor_config->persistent_config.microsteps);
}


/*
    The motors' task, every tick: the two speeds, then the feed of the step pulses. The PIO makes only the steps it
    is fed (ADR-012): if this task stops, whatever stopped it, the motors stop within a few milliseconds.
*/
static void motor_control_task(void * p) {
    TickType_t last_wake_tick = xTaskGetTickCount();
    const float dt_s = portTICK_PERIOD_MS / 1000.0f;

    while (true) {
        motor_tick(&coarse_trickler_motor_config, dt_s);
        motor_tick(&fine_trickler_motor_config, dt_s);
        step_pulses_feed();

        vTaskDelayUntil(&last_wake_tick, 1);
    }
}


// A motor's target, from the trickler's speed. Stored and done: the motors' task takes it from there
static void motor_set_target(motor_config_t * motor_config, float new_velocity) {
    // Not a number: refused, the motor keeps the target it has (audit M4)
    if (!isfinite(new_velocity)) {
        printf("Motor %u: a speed that is not a number is refused\n", motor_config->uart_addr);
        return;
    }

    // Past the motor's limit: held to it
    float max_speed = motor_config->persistent_config.max_speed_rps;
    if (new_velocity > max_speed) {
        new_velocity = max_speed;
    }
    else if (new_velocity < -max_speed) {
        new_velocity = -max_speed;
    }

    motor_config->target_velocity = new_velocity / motor_config->persistent_config.gear_ratio;
}


void motor_set_speed(motor_select_t selected_motor, float new_velocity) {
    if (selected_motor == SELECT_COARSE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        motor_set_target(&coarse_trickler_motor_config, new_velocity);
    }

    if (selected_motor == SELECT_FINE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        motor_set_target(&fine_trickler_motor_config, new_velocity);
    }
}


float motor_get_velocity(motor_select_t selected_motor) {
    // The speed the motor turns at now, ramps included, at the trickler: as motor_set_speed takes it
    return motor_get_shaft_velocity(selected_motor) * motor_get_config(selected_motor)->persistent_config.gear_ratio;
}


float motor_get_shaft_velocity(motor_select_t selected_motor) {
    return motor_get_config(selected_motor)->velocity;
}


void motor_enable(motor_select_t selected_motor, bool enable) {
    if (selected_motor == SELECT_COARSE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        bool en_signal = coarse_trickler_motor_config.persistent_config.inverted_enable ? enable : !enable;

        gpio_put(coarse_trickler_motor_config.en_pin, en_signal);

        // If disabled, we shall also disable the stepper signal
        if (!enable) {
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);
        }
    }

    if (selected_motor == SELECT_FINE_TRICKLER_MOTOR || selected_motor == SELECT_BOTH_MOTOR) {
        bool en_signal = fine_trickler_motor_config.persistent_config.inverted_enable ? enable : !enable;

        gpio_put(fine_trickler_motor_config.en_pin, en_signal);

        // If disabled, we shall also disable the stepper signal
        if (!enable) {
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
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


/*
    Read what one driver reports of itself, and keep the first fault found (ADR-012, step 4). A driver that is absent
    is left alone: motors_ready() tries it again when a mode starts.
*/
static void motor_driver_poll(motor_config_t * motor_config) {
    tmc2209_t * driver = (tmc2209_t *) motor_config->tmc_driver;
    int idx = motor_config->pulse_channel;
    uint32_t drv_status = 0;
    uint32_t gstat = 0;
    bool answered = false;

    if (!driver->ready) {
        return;
    }

    // A fault flag is taken when it is still there on the third reading
    for (int attempt = 0; attempt < MOTOR_STATUS_FAULT_READS; attempt += 1) {
        motor_driver_lock();
        answered = tmc2209_read_status(driver, &drv_status, &gstat);
        motor_driver_unlock();
        if (!answered) {
            break;
        }

        drv_status |= motor_driver_injected_drv_status[idx];
        gstat |= motor_driver_injected_gstat[idx];
        if ((drv_status & (TMC2209_DRV_STATUS_SHORT_MASK | TMC2209_DRV_STATUS_OT)) == 0 &&
            (gstat & (TMC2209_GSTAT_RESET | TMC2209_GSTAT_UV_CP)) == 0) {
            break;
        }
    }

    if (!answered) {
        motor_driver_missed_polls[idx] += 1;
        if (motor_driver_missed_polls[idx] >= MOTOR_STATUS_MISSED_POLLS) {
            printf("Motor driver %u: no answer to %u readings in a row\n", motor_config->uart_addr, MOTOR_STATUS_MISSED_POLLS);
            driver->ready = false;
            motor_driver_faults[idx] = MOTOR_DRIVER_ABSENT;
        }
        return;
    }
    motor_driver_missed_polls[idx] = 0;

    // The temperature warnings go to the log when they change
    if ((drv_status ^ motor_driver_last_drv_status[idx]) & TMC2209_DRV_STATUS_WARN_MASK) {
        printf("Motor driver %u: temperature flags now %03lx (DRV_STATUS %08lx)\n", motor_config->uart_addr,
               (unsigned long) (drv_status & TMC2209_DRV_STATUS_WARN_MASK), (unsigned long) drv_status);
    }
    motor_driver_last_drv_status[idx] = drv_status;

    motor_driver_fault_t fault = MOTOR_DRIVER_OK;
    if (gstat & TMC2209_GSTAT_RESET) {
        fault = MOTOR_DRIVER_RESET;
    }
    else if (drv_status & TMC2209_DRV_STATUS_SHORT_MASK) {
        fault = MOTOR_DRIVER_SHORT;
    }
    else if (drv_status & TMC2209_DRV_STATUS_OT) {
        fault = MOTOR_DRIVER_HOT;
    }
    else if (gstat & TMC2209_GSTAT_UV_CP) {
        fault = MOTOR_DRIVER_SUPPLY;
    }
    if (fault == MOTOR_DRIVER_OK) {
        return;
    }

    if (motor_driver_faults[idx] == MOTOR_DRIVER_OK) {
        printf("Motor driver %u: %s (DRV_STATUS %08lx, GSTAT %lx)\n", motor_config->uart_addr,
               motor_driver_fault_words[fault], (unsigned long) drv_status, (unsigned long) gstat);
        motor_driver_faults[idx] = fault;
    }

    if (fault == MOTOR_DRIVER_RESET) {
        // Its registers are its own defaults again: written anew at once, which also clears the flag. The fault
        // stays, for the charge it came in
        motor_driver_injected_gstat[idx] &= ~TMC2209_GSTAT_RESET;
        driver_setup(motor_config);
    }
    else if (gstat != 0) {
        motor_driver_lock();
        tmc2209_clear_gstat(driver, gstat);
        motor_driver_unlock();
    }
}


// The drivers read while the unit runs (ADR-012, step 4)
static void motor_status_task(void * p) {
    TickType_t last_wake_tick = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(MOTOR_STATUS_PERIOD_MS));

        motor_driver_poll(&coarse_trickler_motor_config);
        motor_driver_poll(&fine_trickler_motor_config);
    }
}


const char * motor_driver_get_status_string(motor_select_t selected_motor) {
    return motor_driver_fault_words[motor_driver_faults[motor_get_config(selected_motor)->pulse_channel]];
}


bool motor_driver_inject_fault(motor_select_t selected_motor, const char * fault) {
    int idx = motor_get_config(selected_motor)->pulse_channel;
    uint32_t drv_status = 0;
    uint32_t gstat = 0;

    if (strcmp(fault, "short") == 0) {
        drv_status = TMC2209_DRV_STATUS_S2GA;
        gstat = TMC2209_GSTAT_DRV_ERR;
    }
    else if (strcmp(fault, "hot") == 0) {
        drv_status = TMC2209_DRV_STATUS_OT | TMC2209_DRV_STATUS_OTPW;
        gstat = TMC2209_GSTAT_DRV_ERR;
    }
    else if (strcmp(fault, "reset") == 0) {
        gstat = TMC2209_GSTAT_RESET;
    }
    else if (strcmp(fault, "supply") == 0) {
        gstat = TMC2209_GSTAT_UV_CP;
    }
    else if (strcmp(fault, "none") != 0) {
        return false;
    }

    motor_driver_injected_drv_status[idx] = drv_status;
    motor_driver_injected_gstat[idx] = gstat;
    return true;
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

    if (xTaskCreate(motor_control_task, "Motors", configMINIMAL_STACK_SIZE, NULL, MOTOR_TASK_PRIORITY, NULL) != pdPASS) {
        printf("Unable to create the motors' task\n");
    }
    if (xTaskCreate(motor_status_task, "Motor Status", 2 * configMINIMAL_STACK_SIZE, NULL, MOTOR_STATUS_TASK_PRIORITY, NULL) != pdPASS) {
        printf("Unable to create the motor drivers' status task\n");
    }
}


void motors_fault_stop(void) {
    // Register writes only: the pulses off, then the drivers
    step_pulses_fault_stop();
    gpio_put(coarse_trickler_motor_config.en_pin, coarse_trickler_motor_config.persistent_config.inverted_enable ? false : true);
    gpio_put(fine_trickler_motor_config.en_pin, fine_trickler_motor_config.persistent_config.inverted_enable ? false : true);
}


void motor_get_step_stats(motor_select_t selected_motor, step_pulses_stats_t * stats) {
    step_pulses_get_stats(motor_get_config(selected_motor)->pulse_channel, stats);
}


float motor_get_step_rate(motor_select_t selected_motor) {
    return step_pulses_get_rate(motor_get_config(selected_motor)->pulse_channel);
}


const char * motors_get_fault(void) {
    if (!motor_config_loaded) {
        return "No motor settings";
    }
    for (int idx = 0; idx < 2; idx += 1) {
        if (motor_driver_faults[idx] != MOTOR_DRIVER_OK) {
            return motor_driver_fault_titles[idx][motor_driver_faults[idx]];
        }
    }
    return NULL;
}


bool motors_ready(void) {
    motor_config_t * motor_configs[] = {&coarse_trickler_motor_config, &fine_trickler_motor_config};

    for (int idx = 0; idx < 2; idx += 1) {
        // A driver that did not answer at start-up may by now: its supply can come after the Pico's. And a fault
        // from before is looked at afresh: the driver is set up again, then read
        if (motor_driver_faults[idx] != MOTOR_DRIVER_OK) {
            if (driver_setup(motor_configs[idx])) {
                motor_driver_faults[idx] = MOTOR_DRIVER_OK;
                motor_driver_poll(motor_configs[idx]);
            }
        }
    }

    return motors_get_fault() == NULL;
}


bool motor_driver_setup(motor_select_t selected_motor) {
    return driver_setup(motor_get_config(selected_motor));
}


void motor_driver_set_lost(motor_select_t selected_motor, bool lost) {
    tmc2209_t * driver = motor_get_driver(selected_motor);
    uint8_t addr = selected_motor == SELECT_FINE_TRICKLER_MOTOR ? FINE_MOTOR_ADDR : COARSE_MOTOR_ADDR;

    // Address 3 is nobody's on this board: asked there, no driver answers
    motor_driver_lock();
    driver->bus.addr = lost ? 3 : addr;
    driver->bus.ifcnt_known = false;
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
    bool changed = false;

    // The settings as they would be: taken only if every one of them is in range
    motor_persistent_config_t new_config = motor_config->persistent_config;

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "m0") == 0) {
            new_config.angular_acceleration = strtof(values[idx], NULL);
            changed = true;
        }
        else if (strcmp(params[idx], "m1") == 0) {
            new_config.full_steps_per_rotation = strtol(values[idx], NULL, 10);
            changed = true;
        }
        else if (strcmp(params[idx], "m2") == 0) {
            new_config.current_ma = (uint16_t) atoi(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "m3") == 0) {
            new_config.microsteps = (uint16_t) atoi(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "m4") == 0) {
            new_config.max_speed_rps = (uint16_t) atoi(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "m5") == 0) {
            new_config.r_sense = (uint16_t) atoi(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "m6") == 0) {
            new_config.min_speed_rps = strtof(values[idx], NULL);
            changed = true;
        }
        else if (strcmp(params[idx], "m7") == 0) {
            new_config.gear_ratio = strtof(values[idx], NULL);
            changed = true;
        }
        else if (strcmp(params[idx], "m8") == 0) {
            new_config.inverted_enable = string_to_boolean(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "m9") == 0) {
            new_config.inverted_direction = string_to_boolean(values[idx]);
            changed = true;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    if (changed) {
        if (!motor_persistent_config_is_valid(&new_config)) {
            // Refused whole: the reply shows the settings as they still are, and nothing is stored
            printf("Motor %u: a setting out of range is refused\n", motor_config->uart_addr);
            return;
        }

        // What the driver holds is written when it changes, not at the next start (ADR-012, fault 5)
        bool driver_changed = new_config.current_ma != motor_config->persistent_config.current_ma ||
                              new_config.microsteps != motor_config->persistent_config.microsteps ||
                              new_config.r_sense != motor_config->persistent_config.r_sense;

        motor_config->persistent_config = new_config;

        if (driver_changed) {
            driver_setup(motor_config);
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
