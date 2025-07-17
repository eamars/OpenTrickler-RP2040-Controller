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
#include "mui.h"
#include "mui_u8g2.h"

#include "configuration.h"
#include "gpio_irq_handler.h"
#include "mini_12864_module.h"
#include "http_rest.h"
#include "eeprom.h"
#include "common.h"
#include "mini_12864_module.h"
#include "display.h"


// Configs
mini_12864_module_config_t mini_12864_module_config;

// Statics (to be shared between IRQ and tasks)
QueueHandle_t encoder_event_queue = NULL;

// Local variables
extern u8g2_t display_handler;

// External variables
extern muif_t muif_list[];
extern fds_t fds_data[];
extern const size_t muif_cnt;



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

void _isr_on_button_enc_update(uint gpio, uint32_t event) {
    static TickType_t last_call_time = 0;
    const TickType_t debounce_timeout_ticks = pdMS_TO_TICKS(250);

    // Button debounce
    TickType_t current_call_time = xTaskGetTickCountFromISR();
     if ((current_call_time - last_call_time) > debounce_timeout_ticks) {
        ButtonEncoderEvent_t button_encoder_event = BUTTON_ENCODER_PRESSED;
        xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
    }
    last_call_time = current_call_time;
}

void _isr_on_button_rst_update(uint gpio, uint32_t event) {
    static TickType_t last_call_time = 0;
    const TickType_t debounce_timeout_ticks = pdMS_TO_TICKS(250);

    // Button debounce
    TickType_t current_call_time = xTaskGetTickCountFromISR();
    if ((current_call_time - last_call_time) > debounce_timeout_ticks) {
        ButtonEncoderEvent_t button_encoder_event = BUTTON_RST_PRESSED;
        xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
    }
    last_call_time = current_call_time;
}


bool mini_12864_module_config_save() {
    bool is_ok = eeprom_write(EEPROM_MINI_12864_CONFIG_BASE_ADDR, (uint8_t *) &mini_12864_module_config, sizeof(mini_12864_module_config));
    return is_ok;
}


bool mini_12864_module_init() {
    bool is_ok;

    // Read configuration
    memset(&mini_12864_module_config, 0x0, sizeof(mini_12864_module_config));
    is_ok = eeprom_read(EEPROM_MINI_12864_CONFIG_BASE_ADDR, (uint8_t *)&mini_12864_module_config, sizeof(mini_12864_module_config));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_MINI_12864_CONFIG_BASE_ADDR);
        return false;
    }

    if (mini_12864_module_config.data_rev != EEPROM_MINI_12864_MODULE_DATA_REV) {
        mini_12864_module_config.data_rev = EEPROM_MINI_12864_MODULE_DATA_REV;

        // Set default
        mini_12864_module_config.inverted_encoder_direction = false;
        mini_12864_module_config.display_rotation = DISPLAY_ROTATION_0;

        // Write back
        is_ok = mini_12864_module_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_MINI_12864_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Register to eeprom save all
    eeprom_register_handler(mini_12864_module_config_save);

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

    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN1, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN2, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    irq_handler.register_interrupt(BUTTON0_ENC_PIN, gpio_irq_handler::irq_event::fall, _isr_on_button_enc_update);
    irq_handler.register_interrupt(BUTTON0_RST_PIN, gpio_irq_handler::irq_event::fall, _isr_on_button_rst_update);

    encoder_event_queue = xQueueCreate(5, sizeof(ButtonEncoderEvent_t));
    if (encoder_event_queue == 0) {
        assert(false);
    }

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

    // Initialize screen rotation
    const u8g2_cb_t *u8g2_cb;
    switch (mini_12864_module_config.display_rotation)
    {
        case DISPLAY_ROTATION_90:
            u8g2_cb = U8G2_R1;
            break;
        case DISPLAY_ROTATION_180:
            u8g2_cb = U8G2_R2;
            break;
        case DISPLAY_ROTATION_270:
            u8g2_cb = U8G2_R3;
            break;
        case DISPLAY_ROTATION_0:
        default:  // default is no rotation
            u8g2_cb = U8G2_R0;
            break;
    }
    u8g2_SetDisplayRotation(&display_handler, u8g2_cb);

    // Clear 
    u8g2_ClearBuffer(&display_handler);
    u8g2_ClearDisplay(&display_handler);

    // u8g2_SetMaxClipWindow(&display_handler);
    // u8g2_SetFont(&display_handler, u8g2_font_6x13_tr);
    // u8g2_DrawStr(&display_handler, 20, 20, "Hello");
    // u8g2_UpdateDisplay(&display_handler);

    printf("done\n");
}



bool http_rest_button_control(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char button_control_json_buffer[256];
    memset(button_control_json_buffer, 0x0, sizeof(button_control_json_buffer));

    strcat(button_control_json_buffer, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"button_pressed\":[");

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "CW") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_ROTATE_CW;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"CW\",");
            }
        }
        
        if (strcmp(params[idx], "CCW") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_ROTATE_CCW;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"CCW\",");
            }
        }

        if (strcmp(params[idx], "PRESS") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"PRESS\",");
            }
        }

        if (strcmp(params[idx], "RST") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_RST_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"RST\",");
            }
        }
    }

    // Remove trailing comma
    size_t len = strlen(button_control_json_buffer);
    if (button_control_json_buffer[len-1] == ',') {
        button_control_json_buffer[len-1] = 0;
    }

    strcat(button_control_json_buffer, "]}");

    // Send to client
    len = strlen(button_control_json_buffer);

    file->data = button_control_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_mini_12864_module_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // b0 (bool): inverted_encoder_direction
    // ee (bool): save to eeprom
    static char buf[128];
    bool save_to_eeprom = false;

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "b0") == 0) {
            bool inverted_encoder_direction = string_to_boolean(values[idx]);
            mini_12864_module_config.inverted_encoder_direction = inverted_encoder_direction;
        }
        if (strcmp(params[idx], "b1") == 0) {
            display_rotation_t display_rotation = (display_rotation_t) atoi(values[idx]);
            mini_12864_module_config.display_rotation = display_rotation;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        mini_12864_module_config_save();
    }

    // Response
    snprintf(buf, sizeof(buf), 
             "%s"
             "{\"b0\":%s, \"b1\":%d}", 
             http_json_header,
             boolean_to_string(mini_12864_module_config.inverted_encoder_direction),
             mini_12864_module_config.display_rotation);
    
    size_t response_len = strlen(buf);
    file->data = buf;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}