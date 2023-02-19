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
#include "pico/cyw43_arch.h"


extern void button_init(void);
extern void button_task(void *p);

extern void display_init(void);
extern void menu_task(void *p);

extern void scale_measurement_init(void);
extern void scale_measurement_generator(void *p);

extern "C"{
    extern void motors_init(void);
}



void watchdog_task(void *p){
    // watchdog_enable(500, true);  // 500ms, enable debug
    bool led_state = true;
    while (true){
        TickType_t last_measurement_tick = xTaskGetTickCount();
        // watchdog_update();

        // Change LED state
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

        led_state = !led_state;

        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(400));
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
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("WiFi Init Failed\n");
        return -1;
    }

    // Configure Neopixel (WS2812)
    printf("Configure Neopixel\n");
    uint ws2812_sm = pio_claim_unused_sm(pio0, true);
    uint ws2812_offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, ws2812_sm, ws2812_offset, NEOPIXEL_PIN, 800000, true);
    put_pixel(urgb_u32(0x00, 0x00, 0x00));  // Encoder RGB1
    put_pixel(urgb_u32(0x00, 0x00, 0x00));  // Encoder RGB2
    put_pixel(urgb_u32(0xff, 0x00, 0x00));  // 12864 Backlight

    // Configure others
    display_init();
    button_init();
    scale_measurement_init();
    motors_init();

    xTaskCreate(watchdog_task, "Watchdog Task", 256, NULL, 2, NULL);
    // xTaskCreate(button_task, "Button Task", 256, NULL, 1, NULL);
    xTaskCreate(menu_task, "Menu Task", 256, NULL, 1, NULL);
    xTaskCreate(scale_measurement_generator, "Mocked Scale Data Generator Task", 128, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}


void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    ( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
#if mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0
    {
        /* The full demo includes a software timer demo/test that requires
        prodding periodically from the tick interrupt. */
        #if (mainENABLE_TIMER_DEMO == 1)
        vTimerPeriodicISRTests();
        #endif

        /* Call the periodic queue overwrite from ISR demo. */
        #if (mainENABLE_QUEUE_OVERWRITE == 1)
        vQueueOverwritePeriodicISRDemo();
        #endif

        /* Call the periodic event group from ISR demo. */
        #if (mainENABLE_EVENT_GROUP == 1)
        vPeriodicEventGroupsProcessing();
        #endif

        /* Call the code that uses a mutex from an ISR. */
        #if (mainENABLE_INTERRUPT_SEMAPHORE == 1)
        vInterruptSemaphorePeriodicTest();
        #endif

        /* Call the code that 'gives' a task notification from an ISR. */
        #if (mainENABLE_TASK_NOTIFY == 1)
        xNotifyTaskFromISR();
        #endif
    }
#endif
}