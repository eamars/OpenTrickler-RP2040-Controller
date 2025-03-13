/*
 * LED blink with FreeRTOS
 */

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "FreeRTOSConfig.h"
#include "configuration.h"
#include "u8g2.h"

// modules
#include "app.h"
#include "motors.h"
#include "eeprom.h"
#include "scale.h"
#include "display.h"
#include "charge_mode.h"
#include "rest_endpoints.h"
#include "wireless.h"
#include "neopixel_led.h"
#include "mini_12864_module.h"
#include "menu.h"
#include "profile.h"
#include "servo_gate.h"


int main()
{
    // stdio_init_all();
    // Initialize EEPROM first
    eeprom_init();

    // Initialize Neopixel RGB on the mini 12864 board
    neopixel_led_init();

    // Configure other functions from mini 12864 display
    mini_12864_module_init();

    // Initialize wireless settings
    wireless_init();

    // Load config for motors
    motor_init_err_t motor_init_err = motors_init();
    if (motor_init_err != MOTOR_INIT_OK) {
        handle_motor_init_error(motor_init_err);
    }

    // Initialize UART
    scale_init();

    // Initialize charge mode settings
    charge_mode_config_init();

    // Initialize profile data
    profile_data_init();

    // Initialize the servo
    servo_gate_init();

    // Start menu task
    xTaskCreate(menu_task, "Menu Task", 1024, NULL, 6, NULL);

    // Start RTOS
    vTaskStartScheduler();

    while (true)
        ;
}
