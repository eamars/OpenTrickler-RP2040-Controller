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

#include "configuration.h"
#include "gpio_irq_handler.h"
#include "mini_12864_module.h"
#include "eeprom.h"
#include "common.h"
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
// time of the last one taken is a bounce; the level read says which edge it is. No state is kept here: the menu drops
// a repeat, and reads the pin again before a hold counts, so an edge lost in a bounce costs a click, never a Back
void _isr_on_button_enc_update(uint gpio, uint32_t event) {
    static TickType_t last_edge_tick = 0;
    const TickType_t debounce_ticks = pdMS_TO_TICKS(20);

    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - last_edge_tick) < debounce_ticks) {
        return;
    }
    last_edge_tick = now;
    bool pressed = !gpio_get(BUTTON0_ENC_PIN);      // pulled up: low while pressed
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


bool button_enc_is_down(void) {
    return !gpio_get(BUTTON0_ENC_PIN);
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

    // Rotation (ADR-010, gate 4): the panel turns itself, 180 degrees by its flip. At 90 or 270 a 128 x 64 panel is
    // 64 pixels wide and no row fits, so they are shown as 0 and 180 (menu_design.md, item 11)
    display_rotation_t rotation = mini_12864_module_config.display_rotation;
    bool flipped = rotation == DISPLAY_ROTATION_180 || rotation == DISPLAY_ROTATION_270;
    if (rotation == DISPLAY_ROTATION_90 || rotation == DISPLAY_ROTATION_270) {
        printf("Display: %d degrees is shown as %d: the 12864 turns 0 and 180 only\n", (int) rotation * 90,
               flipped ? 180 : 0);
    }
    printf("Initializing Display -- ");
    display_init(flipped);
    printf("done\n");

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
