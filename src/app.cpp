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


#define DISPlAY0_SPI spi0
#define DISPLAY0_RX_PIN PICO_DEFAULT_SPI_RX_PIN
#define DISPLAY0_TX_PIN PICO_DEFAULT_SPI_TX_PIN
#define DISPLAY0_CS_PIN PICO_DEFAULT_SPI_CSN_PIN
#define DISPLAY0_SCK_PIN PICO_DEFAULT_SPI_SCK_PIN
#define DISPLAY0_A0_PIN 20
#define DISPLAY0_RESET_PIN 21

#define BUTTON0_ENCODER_PIN1 15
#define BUTTON0_ENCODER_PIN2 14
#define BUTTON0_SW_PIN 22

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


void button_task(void *p){
    gpio_init(BUTTON0_ENCODER_PIN1);
    gpio_set_dir(BUTTON0_ENCODER_PIN1, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN1);

    gpio_init(BUTTON0_ENCODER_PIN2);
    gpio_set_dir(BUTTON0_ENCODER_PIN2, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN2);

    gpio_init(BUTTON0_SW_PIN);
    gpio_set_dir(BUTTON0_SW_PIN, GPIO_IN);
    gpio_pull_up(BUTTON0_SW_PIN);

    portTickType xLastWakeTime = xTaskGetTickCount();

    while (true){
        bool encoder_pin1_state = gpio_get(BUTTON0_ENCODER_PIN1);
        bool encoder_pin2_state = gpio_get(BUTTON0_ENCODER_PIN2);
        bool button_sw_state = gpio_get(BUTTON0_SW_PIN);
        printf("%d %d %d\n", encoder_pin1_state, encoder_pin2_state, button_sw_state);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}



uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch(msg)
    {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:	// called once during init phase of u8g2/u8x8
            // We don't initialize here
            break;							// can be used to setup pins
        case U8X8_MSG_DELAY_NANO:			// delay arg_int * 1 nano second
            break;    
        case U8X8_MSG_DELAY_100NANO:		// delay arg_int * 100 nano seconds
            __asm volatile ("NOP\n");
            break;
        case U8X8_MSG_DELAY_10MICRO:		// delay arg_int * 10 micro seconds
            sleep_us((uint64_t) arg_int * 10);
            break;
        case U8X8_MSG_DELAY_MILLI:			// delay arg_int * 1 milli second
        {
            BaseType_t scheduler_state = xTaskGetSchedulerState();
            if (scheduler_state == taskSCHEDULER_RUNNING){
                vTaskDelay(arg_int / portTICK_PERIOD_MS);
            }
            else {
                sleep_ms(arg_int);
            }
            break;
        }
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
            gpio_put(DISPLAY0_CS_PIN, arg_int);
            break;
        case U8X8_MSG_GPIO_DC:				// DC (data/cmd, A0, register select) pin: Output level in arg_int
            gpio_put(DISPLAY0_A0_PIN, arg_int);
            break;
        case U8X8_MSG_GPIO_RESET:			// Reset pin: Output level in arg_int
            gpio_put(DISPLAY0_RESET_PIN, arg_int);
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
            spi_write_blocking(DISPlAY0_SPI, (uint8_t *) arg_ptr, arg_int);
            break;
        case U8X8_MSG_BYTE_INIT:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;
        case U8X8_MSG_BYTE_SET_DC:
            u8x8_gpio_SetDC(u8x8, arg_int);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);  
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;
        default:
            return 0;
    }
    return 1;
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    return
            ((uint32_t) (g) << 8) |
            ((uint32_t) (r) << 16) |
            ((uint32_t) (w) << 24) |
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
    put_pixel(urgb_u32(0x00, 0x00, 0x00, 0x00));  // Encoder RGB1
    put_pixel(urgb_u32(0x00, 0x00, 0x00, 0x00));  // Encoder RGB2
    put_pixel(urgb_u32(0xf0, 0xf0, 0xf0, 0x00));  // 12864 Backlight

    // Configure 12864
    printf("Initialize LED display\n");
    // Initialize SPI engine
    spi_init(DISPlAY0_SPI, 4000 * 1000);

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

    // Configure property for RESET
    gpio_init(DISPLAY0_RESET_PIN);
    gpio_set_dir(DISPLAY0_RESET_PIN, GPIO_OUT);
    gpio_put(DISPLAY0_RESET_PIN, 0);

    u8g2_t display_handler;
    u8g2_Setup_uc1701_mini12864_f(
        &display_handler, 
        U8G2_R0, 
        u8x8_byte_pico_hw_spi, 
        u8x8_gpio_and_delay
    );

    // Display something
    // Initialize Screen
    u8g2_InitDisplay(&display_handler);
    u8g2_SetPowerSave(&display_handler, 0);
    u8g2_SetContrast(&display_handler, 200);

    // Clear 
    u8g2_ClearBuffer(&display_handler);
    u8g2_ClearDisplay(&display_handler);

    u8g2_SetMaxClipWindow(&display_handler);
    u8g2_SetFont(&display_handler, u8g2_font_6x13_tr);
    u8g2_DrawStr(&display_handler, 20, 20, "Hello");
    u8g2_UpdateDisplay(&display_handler);

    printf("Start LED blink\n");

    struct led_task_arg arg1 = { CYW43_WL_GPIO_LED_PIN, 100 };
    xTaskCreate(cyw43_led_task, "LED_Task 1", 256, &arg1, 1, NULL);

    xTaskCreate(button_task, "Button Task", 256, NULL, 1, NULL);

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