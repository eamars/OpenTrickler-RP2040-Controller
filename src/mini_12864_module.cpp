// 
//            -----
//        5V |10  9 | GND
//        -- | 8  7 | --
//    (DIN)  | 6  5#| (RESET)
//  (LCD_A0) | 4  3 | (LCD_CS)
// (BTN_ENC) | 2  1 | --
//            ------
//             EXP1
//            -----
//        5V |10  9 | GND
//   (RESET) | 8  7 | --
//   (MOSI)  | 6  5#| (EN2)
//        -- | 4  3 | (EN1)
//  (LCD_SCK)| 2  1 | --
//            ------
//             EXP2
// 
// For Pico W
// EXP1_2 (BTN_ENC) <-> PIN29 (GP22)
// EXP2_5 (ENCODER2) <-> PIN19 (GP14)
// EXP2_3 (ENCODER1) <-> PIN20 (GP15)
// EXP2_8 (BTN_RST) <-> PIN16 (GP12)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "pico/stdlib.h"
#include "u8g2.h"

#include "configuration.h"
#include "gpio_irq_handler.h"
#include "mini_12864_module.h"
#include "eeprom.h"
#include "common.h"
#include "mini_12864_module.h"
#include "display.h"


// Configs
mini_12864_module_config_t mini_12864_module_config;
const mini_12864_module_config_t default_mini_12864_module_config = {
    .data_rev = 0,
    .inverted_encoder_direction = false,
    .display_rotation = DISPLAY_ROTATION_0,
};

// Statics (to be shared between IRQ and tasks)
QueueHandle_t encoder_event_queue = NULL;

// Local variables
extern u8g2_t display_handler;



void _isr_on_encoder_update(uint gpio, uint32_t event){
    static uint8_t state = 2;
    static int8_t count = 0;

    bool en1 = gpio_get(BUTTON0_ENCODER_PIN1);
    bool en2 = gpio_get(BUTTON0_ENCODER_PIN2);

    switch (state) {
        case 0: {
            if (en1) {
                count += 1;
                state = 1;
            }
            else if (en2) {
                count -= 1;
                state = 3;
            }
            break;
        }
        case 1: {
            if (!en1) {
                count -= 1;
                state = 0;
            }
            else if (en2) {
                count += 1;
                state = 2;
            }
            break;
        }
        case 2: {
            if (!en1) {
                count += 1;
                state = 3;
            }
            else if (!en2) {
                count -= 1;
                state = 1;
            }
            break;
        }
        case 3: {
            if (!en1) {
                count += 1;
                state = 0;
            }
            else if (en2) {
                count -= 1;
                state = 2;
            }
            break;
        }
        default:
            break;
    }

    ButtonEncoderEvent_t button_encoder_event;
    if (count >= 4) {
        count = 0;

        if (mini_12864_module_config.inverted_encoder_direction) {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
        }
        else {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
        }
        
        if (encoder_event_queue) {
            xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
        }
    }
    else if (count <= -4) {
        count = 0;

        if (mini_12864_module_config.inverted_encoder_direction) {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
        }
        else {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
        }

        if (encoder_event_queue) {
            xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
        }
    }
}

// The knob's button, down and up (ADR-010, gate 4): the menu tells a click from a hold. An edge within the debounce
// time of the last one taken is a bounce; the level read says which edge it is, and an edge that repeats the last
// one taken (a bounce that ended on the same level) is dropped
void _isr_on_button_enc_update(uint gpio, uint32_t event) {
    static TickType_t last_edge_tick = 0;
    static bool down = false;
    const TickType_t debounce_ticks = pdMS_TO_TICKS(20);

    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - last_edge_tick) < debounce_ticks) {
        return;
    }
    bool pressed = !gpio_get(BUTTON0_ENC_PIN);      // pulled up: low while pressed
    if (pressed == down) {
        return;
    }
    last_edge_tick = now;
    down = pressed;
    ButtonEncoderEvent_t button_encoder_event = pressed ? BUTTON_ENCODER_DOWN : BUTTON_ENCODER_UP;
    if (encoder_event_queue) {
        xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
    }
}

void _isr_on_button_rst_update(uint gpio, uint32_t event) {
    static TickType_t last_call_time = 0;
    const TickType_t debounce_timeout_ticks = pdMS_TO_TICKS(250);

    // Button debounce
    TickType_t current_call_time = xTaskGetTickCountFromISR();
    if ((current_call_time - last_call_time) > debounce_timeout_ticks) {
        ButtonEncoderEvent_t button_encoder_event = BUTTON_RST_PRESSED;
        if (encoder_event_queue) {
            xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
        }
    }
    last_call_time = current_call_time;
}


bool mini_12864_module_config_save() {
    bool is_ok = save_config(EEPROM_MINI_12864_CONFIG_BASE_ADDR, &mini_12864_module_config, sizeof(mini_12864_module_config));
    return is_ok;
}


bool mini_12864_module_init() {
    bool is_ok;

    // Read configuration
    memset(&mini_12864_module_config, 0x0, sizeof(mini_12864_module_config));
    is_ok = load_config(EEPROM_MINI_12864_CONFIG_BASE_ADDR, &mini_12864_module_config, &default_mini_12864_module_config, sizeof(mini_12864_module_config), EEPROM_MINI_12864_MODULE_DATA_REV);
    if (!is_ok) {
        printf("Unable to read mini12864 configuration\n");
        return false;
    }

    // Register to eeprom save all
    eeprom_register_handler(mini_12864_module_config_save);
    mini_12864_settings_register();

    // Run subsequent function inits
    button_init();
    display_init();

    return is_ok;
}
  

void button_init() {
    printf("Initializing Button Task -- ");

    // Configure button encoder
    gpio_init(BUTTON0_ENCODER_PIN1);
    gpio_set_dir(BUTTON0_ENCODER_PIN1, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN1);

    gpio_init(BUTTON0_ENCODER_PIN2);
    gpio_set_dir(BUTTON0_ENCODER_PIN2, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN2);

    // Configure button encoder
    gpio_init(BUTTON0_ENC_PIN);
    gpio_set_dir(BUTTON0_ENC_PIN, GPIO_IN);
    gpio_pull_up(BUTTON0_ENC_PIN);

    // // Configure button reset
    gpio_init(BUTTON0_RST_PIN);
    gpio_set_dir(BUTTON0_RST_PIN, GPIO_IN);
    gpio_pull_up(BUTTON0_RST_PIN);

    // The queue first: an interrupt in the first moments of a start writes to it (ADR-012's audit, ADR-010)
    encoder_event_queue = xQueueCreate(8, sizeof(ButtonEncoderEvent_t));
    if (encoder_event_queue == 0) {
        assert(false);
    }

    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN1, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN2, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    // Both edges of the knob's button: down and up (ADR-010, gate 4)
    irq_handler.register_interrupt(BUTTON0_ENC_PIN, gpio_irq_handler::irq_event::change, _isr_on_button_enc_update);
    irq_handler.register_interrupt(BUTTON0_RST_PIN, gpio_irq_handler::irq_event::fall, _isr_on_button_rst_update);

    printf("done\n");
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
            busy_wait_us(arg_int * 10ULL);
            break;
        case U8X8_MSG_DELAY_MILLI:			// delay arg_int * 1 milli second
        {
            BaseType_t scheduler_state = xTaskGetSchedulerState();
            delay_ms(arg_int, scheduler_state);
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
    switch (msg)
    {
        case U8X8_MSG_BYTE_SEND:
            // taskENTER_CRITICAL();
            spi_write_blocking(DISPLAY0_SPI, (uint8_t *) arg_ptr, arg_int);
            // taskEXIT_CRITICAL();
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


void display_init() {
    // Configure 12864
    printf("Initializing Display Task -- ");
    // Initialize SPI engine
    spi_init(DISPLAY0_SPI, 4000 * 1000);

    // Configure port for SPI
    // gpio_set_function(DISPLAY0_RX_PIN, GPIO_FUNC_SPI);  // Rx
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

    // Initialize driver
    u8g2_Setup_uc1701_mini12864_f(
        &display_handler, 
        U8G2_R0, 
        u8x8_byte_pico_hw_spi, 
        u8x8_gpio_and_delay
    );

    // Initialize Screen
    u8g2_InitDisplay(&display_handler);
    u8g2_SetPowerSave(&display_handler, 0);
    u8g2_SetContrast(&display_handler, 255);

    // Rotation (ADR-010, gate 4): LVGL fills u8g2's buffer whole, which u8g2's own rotation does not turn, so the
    // panel turns itself: 180 degrees by its flip. At 90 or 270 a 128 x 64 panel is 64 pixels wide and no row fits,
    // so they are shown as 0 and 180 (menu_design.md, item 11)
    display_rotation_t rotation = mini_12864_module_config.display_rotation;
    bool flipped = rotation == DISPLAY_ROTATION_180 || rotation == DISPLAY_ROTATION_270;
    if (rotation == DISPLAY_ROTATION_90 || rotation == DISPLAY_ROTATION_270) {
        printf("Display: %d degrees is shown as %d: the 12864 turns 0 and 180 only\n", (int) rotation * 90,
               flipped ? 180 : 0);
    }
    u8g2_SetFlipMode(&display_handler, flipped ? 1 : 0);

    // Clear 
    u8g2_ClearBuffer(&display_handler);
    u8g2_ClearDisplay(&display_handler);

    // u8g2_SetMaxClipWindow(&display_handler);
    // u8g2_SetFont(&display_handler, u8g2_font_6x13_tr);
    // u8g2_DrawStr(&display_handler, 20, 20, "Hello");
    // u8g2_UpdateDisplay(&display_handler);

    printf("done\n");
}


