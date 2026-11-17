/*
 * LED blink with FreeRTOS
 */

#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "FreeRTOSConfig.h"
#include "configuration.h"

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
#include "update_poll.h"
#include "jobs.h"


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

    // The update's channel and source, and its poll (ADR-010, gate 5): it polls once Wi-Fi joins
    if (!update_poll_init()) {
        printf("Update: no room for the poll's lock\n");
    }

    // The job task, where the charge, a cleanup and an update run, then the screen's (ADR-010, gate 2)
    if (!jobs_init()) {
        printf("Jobs: no room for the job task\n");
        assert(false);
    }
    // The menu draws with LVGL: at 1024 words it had 142 left (ADR-010, gate 4). Its locks first: the web and the
    // console read its frame
    menu_init();
    xTaskCreate(menu_task, "Menu Task", 1536, NULL, 6, NULL);

    // The remote control's console task
    rc_console_init();

    // Start RTOS
    vTaskStartScheduler();

    while (true)
        ;
}
