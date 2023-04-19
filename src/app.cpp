/*
 * LED blink with FreeRTOS
 */

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "generated/ws2812.pio.h"
#include "FreeRTOSConfig.h"
#include "configuration.h"
#include "u8g2.h"

// modules
#include "app.h"
#include "motors.h"
#include "eeprom.h"
#include "scale.h"
#include "display.h"


extern void button_init(void);
extern void button_task(void *p);

extern void menu_task(void *p);


extern void scale_measurement_init(void);
extern void scale_measurement_generator(void *p);




void cyw43_task(void *p){
    // Initialize cyw43 and start the background tasks
    if (cyw43_arch_init()) {
        printf("WiFi Init Failed\n");
        exit(-1);
    }

    // watchdog_enable(500, true);  // 500ms, enable debug
    bool led_state = true;
    while (true){
        TickType_t last_measurement_tick = xTaskGetTickCount();
        // watchdog_update();

        // Change LED state
        // cyw43_arch_gpio_put(WATCHDOG_LED_PIN, led_state);

        led_state = !led_state;

        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(500));
    }
}


void watchdog_task(void *p){
    // watchdog_enable(500, true);  // 500ms, enable debug
    bool led_state = true;
    while (true){
        TickType_t last_measurement_tick = xTaskGetTickCount();
        // watchdog_update();

        // Change LED state
        gpio_put(WATCHDOG_LED_PIN, led_state);

        led_state = !led_state;

        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(500));
    }
}



static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (g) << 8) |
            ((uint32_t) (r) << 16) |
            (uint32_t) (b);
}
 
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}


int main()
{
    // stdio_init_all();
    // Initialize EEPROM first
    eeprom_init();

    // Load config for motors
    motor_config_init();

    // Initialize UART
    scale_init();

    // Configure Neopixel (WS2812)
    uint ws2812_sm = pio_claim_unused_sm(pio0, true);
    uint ws2812_offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, ws2812_sm, ws2812_offset, NEOPIXEL_PIN, 800000, true);
    put_pixel(urgb_u32(0x00, 0x00, 0x00));  // Encoder RGB1
    put_pixel(urgb_u32(0xFF, 0x00, 0xFF));  // Encoder RGB2
    put_pixel(urgb_u32(0xff, 0xff, 0xff));  // 12864 Backlight

    // Configure others
    display_init();
    button_init();

#ifdef RASPBERRYPI_PICO_W
    xTaskCreate(cyw43_task, "Cyw43 Task", configMINIMAL_STACK_SIZE, NULL, 10, NULL);
#else
    xTaskCreate(watchdog_task, "Watchdog Task", configMINIMAL_STACK_SIZE, NULL, 10, NULL);
#endif  // RASPBERRYPI_PICO_W
    xTaskCreate(menu_task, "Menu Task", 512, NULL, 6, NULL);
    xTaskCreate(scale_listener_task, "Scale Task", configMINIMAL_STACK_SIZE, NULL, 9, NULL);
    xTaskCreate(motor_task, "Motor Task", configMINIMAL_STACK_SIZE, NULL, 8, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
