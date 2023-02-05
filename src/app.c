/*
 * LED blink with FreeRTOS
 */

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "u8g2.h"
#include "hardware/spi.h"
#include "generated/ws2812.pio.h"


#define DISPLAY0_RX_PIN PICO_DEFAULT_SPI_RX_PIN
#define DISPLAY0_TX_PIN PICO_DEFAULT_SPI_TX_PIN
#define DISPLAY0_CS_PIN PICO_DEFAULT_SPI_CSN_PIN
#define DISPLAY0_SCK_PIN PICO_DEFAULT_SPI_SCK_PIN
#define DISPLAY0_A0_PIN 20

#define NEOPIXEL_PIN 26


struct led_task_arg {
    int gpio;
    int delay;
};


void cyw43_led_task(void *p){
    struct led_task_arg *a = (struct led_task_arg *)p;

    while (true){
        cyw43_arch_gpio_put(a->gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(a->delay));
        cyw43_arch_gpio_put(a->gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(a->delay));
    }
}


uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch(msg)
    {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:	// called once during init phase of u8g2/u8x8
            break;							// can be used to setup pins
        case U8X8_MSG_DELAY_NANO:			// delay arg_int * 1 nano second
            break;    
        case U8X8_MSG_DELAY_100NANO:		// delay arg_int * 100 nano seconds
            break;
        case U8X8_MSG_DELAY_10MICRO:		// delay arg_int * 10 micro seconds
            break;
        case U8X8_MSG_DELAY_MILLI:			// delay arg_int * 1 milli second
            break;
        case U8X8_MSG_DELAY_I2C:				// arg_int is the I2C speed in 100KHz, e.g. 4 = 400 KHz
            break;							// arg_int=1: delay by 5us, arg_int = 4: delay by 1.25us
        case U8X8_MSG_GPIO_D0:				// D0 or SPI clock pin: Output level in arg_int
            //case U8X8_MSG_GPIO_SPI_CLOCK:
            break;
        case U8X8_MSG_GPIO_D1:				// D1 or SPI data pin: Output level in arg_int
            //case U8X8_MSG_GPIO_SPI_DATA:
            break;
        case U8X8_MSG_GPIO_D2:				// D2 pin: Output level in arg_int
        break;
        case U8X8_MSG_GPIO_D3:				// D3 pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_D4:				// D4 pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_D5:				// D5 pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_D6:				// D6 pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_D7:				// D7 pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_E:				// E/WR pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_CS:				// CS (chip select) pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_DC:				// DC (data/cmd, A0, register select) pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_RESET:			// Reset pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_CS1:				// CS1 (chip select) pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_CS2:				// CS2 (chip select) pin: Output level in arg_int
            break;
        case U8X8_MSG_GPIO_I2C_CLOCK:		// arg_int=0: Output low at I2C clock pin
            break;							// arg_int=1: Input dir with pullup high for I2C clock pin
        case U8X8_MSG_GPIO_I2C_DATA:			// arg_int=0: Output low at I2C data pin
            break;							// arg_int=1: Input dir with pullup high for I2C data pin
        case U8X8_MSG_GPIO_MENU_SELECT:
            u8x8_SetGPIOResult(u8x8, /* get menu select pin state */ 0);
            break;
        case U8X8_MSG_GPIO_MENU_NEXT:
            u8x8_SetGPIOResult(u8x8, /* get menu next pin state */ 0);
            break;
        case U8X8_MSG_GPIO_MENU_PREV:
            u8x8_SetGPIOResult(u8x8, /* get menu prev pin state */ 0);
            break;
        case U8X8_MSG_GPIO_MENU_HOME:
            u8x8_SetGPIOResult(u8x8, /* get menu home pin state */ 0);
            break;
        default:
            u8x8_SetGPIOResult(u8x8, 1);			// default return value
        break;
    }
    return 1;
}

uint8_t u8x8_byte_pico_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) 
{
    uint8_t *data;
    uint8_t internal_spi_mode;

    switch (msg)
    {
        case U8X8_MSG_BYTE_SEND:

            break;
        default:
            break;
    }
    return 1;
}
 
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

int main()
{
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("WiFi Init Failed");
        return -1;
    }

    // Configure Neopixel (WS2812)
    printf("Configure Neopixel");
    int sm = 0;
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, sm, offset, NEOPIXEL_PIN, 800000, true);
    put_pixel(5 * 0x10101);

    // Configure 12864
    printf("Initialize LED display\n");
    // Initialize SPI engine
    spi_init(spi0, 4000 * 1000);

    // Configure port for SPI
    gpio_set_function(DISPLAY0_RX_PIN, GPIO_FUNC_SPI);  // Rx
    gpio_set_function(DISPLAY0_SCK_PIN, GPIO_FUNC_SPI);  // CSn
    gpio_set_function(DISPLAY0_SCK_PIN, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(DISPLAY0_TX_PIN, GPIO_FUNC_SPI);  // Tx

    // Configure property for CS
    gpio_init(DISPLAY0_CS_PIN);
    gpio_set_dir(DISPLAY0_CS_PIN, GPIO_OUT);
    gpio_put(DISPLAY0_CS_PIN, 1);

    // Configure property for A0 (D/C)
    gpio_init(DISPLAY0_A0_PIN);
    gpio_set_dir(DISPLAY0_A0_PIN, GPIO_OUT);
    gpio_put(DISPLAY0_A0_PIN, 0);

    u8g2_t display_handler;
    u8g2_Setup_uc1701_mini12864_f(
        &display_handler, 
        U8G2_R0, 
        u8x8_byte_pico_hw_spi, 
        u8x8_gpio_and_delay
    );

    printf("Start LED blink\n");

    struct led_task_arg arg1 = { CYW43_WL_GPIO_LED_PIN, 100 };
    xTaskCreate(cyw43_led_task, "LED_Task 1", 256, &arg1, 1, NULL);

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