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
#include "rotary_button.h"
#include "menu.h"


uint8_t software_reboot() {
    watchdog_reboot(0, 0, 0);

    return 0;
}


int main()
{
    // stdio_init_all();
    // Initialize EEPROM first
    eeprom_init();

    // Initialize Neopixel RGB on the mini 12864 board
    neopixel_led_init();

    // Configure others
    display_init();
    button_init();

    // Load config for motors
    motors_init();

    // Initialize UART
    scale_init();

    // Initialize charge mode settings
    charge_mode_config_init();

    // Load wireless settings
    wireless_config_init();

#ifdef RASPBERRYPI_PICO_W
    xTaskCreate(wireless_task, "Wireless Task", 512, NULL, 3, NULL);
#else
    // xTaskCreate(watchdog_task, "Watchdog Task", configMINIMAL_STACK_SIZE, NULL, 10, NULL);
#endif  // RASPBERRYPI_PICO_W
    xTaskCreate(menu_task, "Menu Task", configMINIMAL_STACK_SIZE, NULL, 6, NULL);
    xTaskCreate(scale_listener_task, "Scale Task", configMINIMAL_STACK_SIZE, NULL, 9, NULL);
    // xTaskCreate(motor_task, "Motor Task", configMINIMAL_STACK_SIZE, NULL, 8, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
