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
#include "activity.h"
#include "app.h"
#include "events.h"
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
#include "rc_console.h"
#include "update.h"


int main()
{
#if RC_TEST_HANG_AT_START
    // A test build (rc_run.py update --expect back): it never starts, and only the boot's own watchdog ends its trial
    while (true) {
        tight_loop_contents();
    }
#endif
    // The console, over RTT through the debug probe (ADR-001): first, so every printf below reaches it
    stdio_init_all();
    printf("OpenTrickler starting\n");

    // What the unit is doing, and the lock its settings' writes share (ADR-003, gate 3), and its event log, which
    // begins with this start (gate 4): before any task
    activity_init();
    events_init();

    // The image slots (ADR-004): on a trial, the watchdog is armed now, before the slow parts of the start-up
    update_init();

    // Initialize EEPROM first
    eeprom_init();

    // Initialize Neopixel RGB on the mini 12864 board
    neopixel_led_init();

    // Configure other functions from mini 12864 display
    mini_12864_module_init();

    // Initialize wireless settings
    wireless_init();

    // The motors and their drivers. A driver that does not answer does not stop the start-up (ADR-012)
    motors_init();

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

    // The remote control's console task
    rc_console_init();

    // Start RTOS
    vTaskStartScheduler();

    while (true)
        ;
}
