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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "chip.h"
#include "chip_gpio.h"
#include "rtos_port.h"

#include "configuration.h"
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
    .inverted_display = false,
    .charge_view = DISPLAY_CHARGE_VIEW_WEIGHT,
};

// The knob's and the buttons' events for the menu: put in by the interrupts, the menu's own wakes and the console
static QueueHandle_t encoder_event_queue = NULL;

// The knob's tap: written by the interrupts, taken by a task on either core, under the RTOS's critical section (its
// lock holds the other core off too)
#define KNOB_TAP_SIZE 128
static knob_tap_entry_t knob_tap[KNOB_TAP_SIZE];
static size_t knob_tap_count = 0;
static uint32_t knob_tap_lost = 0;


// From the interrupts only
static void knob_tap_add(char kind) {
    UBaseType_t saved = rtos_critical_enter_from_isr();
    if (knob_tap_count < KNOB_TAP_SIZE) {
        knob_tap[knob_tap_count].at_us = (uint32_t) chip_time_us();
        knob_tap[knob_tap_count].kind = kind;
        knob_tap_count += 1;
    }
    else {
        knob_tap_lost += 1;
    }
    rtos_critical_exit_from_isr(saved);
}


// An event for the menu, from an interrupt; one the queue has no room for is lost, and the tap says so
static void knob_send_from_isr(ButtonEncoderEvent_t event) {
    if (encoder_event_queue && xQueueSendFromISR(encoder_event_queue, &event, NULL) != pdTRUE) {
        knob_tap_add('q');
    }
}


size_t mini_12864_knob_tap(knob_tap_entry_t *out, size_t max, uint32_t *lost) {
    rtos_critical_enter();
    size_t count = knob_tap_count < max ? knob_tap_count : max;
    memcpy(out, knob_tap, count * sizeof(knob_tap_entry_t));
    *lost = knob_tap_lost + (uint32_t) (knob_tap_count - count);
    knob_tap_count = 0;
    knob_tap_lost = 0;
    rtos_critical_exit();
    return count;
}


bool button_send(ButtonEncoderEvent_t event, uint32_t timeout_ms) {
    return encoder_event_queue != NULL && xQueueSend(encoder_event_queue, &event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}


ButtonEncoderEvent_t button_wait_for_input_ms(uint32_t timeout_ms) {
    ButtonEncoderEvent_t button_encoder_event;
    if (encoder_event_queue == NULL ||
        !xQueueReceive(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(timeout_ms))) {
        return BUTTON_NO_EVENT;
    }
    return button_encoder_event;
}


bool mini_12864_inverted_display(void) {
    return mini_12864_module_config.inverted_display;
}


static void _isr_on_encoder_update(unsigned int pin) {
    static uint8_t state = 2;
    static int8_t count = 0;

    bool en1 = chip_gpio_get(BUTTON0_ENCODER_PIN1);
    bool en2 = chip_gpio_get(BUTTON0_ENCODER_PIN2);

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
        knob_tap_add('r');
        knob_send_from_isr(button_encoder_event);
    }
    else if (count <= -4) {
        count = 0;

        if (mini_12864_module_config.inverted_encoder_direction) {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
        }
        else {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
        }
        knob_tap_add('l');
        knob_send_from_isr(button_encoder_event);
    }
}

// The knob's button (ADR-010): a press is taken as it goes down, so a click acts at once; there is no long press (the
// owner: the button is not reliable enough to time). Both edges are read for the debounce: the button's state changes
// only on an edge whose level differs from it, and an edge within the debounce time of the last change is its bounce
#define KNOB_DEBOUNCE_US 20000u

static void _isr_on_button_enc_update(unsigned int pin) {
    static bool down = false;
    static uint32_t changed_us = 0;

    uint32_t now_us = (uint32_t) chip_time_us();
    bool pressed = !chip_gpio_get(BUTTON0_ENC_PIN);     // pulled up: low while pressed
    if (pressed == down || now_us - changed_us < KNOB_DEBOUNCE_US) {
        knob_tap_add(pressed ? 'X' : 'x');
        return;
    }
    down = pressed;
    changed_us = now_us;
    knob_tap_add(pressed ? 'd' : 'u');
    if (pressed) {
        knob_send_from_isr(BUTTON_ENCODER_PRESSED);
    }
}

static void _isr_on_button_rst_update(unsigned int pin) {
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
    is_ok = load_config(EEPROM_MINI_12864_CONFIG_BASE_ADDR, &mini_12864_module_config, &default_mini_12864_module_config, sizeof(mini_12864_module_config));
    if (!is_ok) {
        printf("Unable to read mini12864 configuration\n");
        return false;
    }

    // Rotation (ADR-010, gate 4): the panel turns itself, 180 degrees by its flip. At 90 or 270 a 128 x 64 panel is
    // 64 pixels wide and no row fits, so a rotation an older firmware stored as 90 or 270 becomes its nearest, 0 or
    // 180, and is stored (menu_design.md, item 11). Before the settings register: their mending would take the first
    // choice, 0, and turn a unit stored at 270 upside down (the review of ADR-010, 2026-11-18)
    display_rotation_t rotation = mini_12864_module_config.display_rotation;
    if (rotation == DISPLAY_ROTATION_90 || rotation == DISPLAY_ROTATION_270) {
        mini_12864_module_config.display_rotation =
            rotation == DISPLAY_ROTATION_270 ? DISPLAY_ROTATION_180 : DISPLAY_ROTATION_0;
        printf("Display: %d degrees stored as %d: the 12864 turns 0 and 180 only\n", (int) rotation * 90,
               (int) mini_12864_module_config.display_rotation * 90);
        mini_12864_module_config_save();
    }

    // Register to eeprom save all
    eeprom_register_handler(mini_12864_module_config_save);
    mini_12864_settings_register();

    // Run subsequent function inits
    button_init();

    printf("Initializing Display -- ");
    display_init(mini_12864_module_config.display_rotation == DISPLAY_ROTATION_180);
    printf("done\n");

    return is_ok;
}
  

void button_init() {
    printf("Initializing Button Task -- ");

    // The knob's two pins, its button and the reset button, each pulled up
    chip_gpio_input(BUTTON0_ENCODER_PIN1, true);
    chip_gpio_input(BUTTON0_ENCODER_PIN2, true);
    chip_gpio_input(BUTTON0_ENC_PIN, true);
    chip_gpio_input(BUTTON0_RST_PIN, true);

    // The queue first: an interrupt in the first moments of a start writes to it (ADR-012's audit, ADR-010)
    encoder_event_queue = xQueueCreate(8, sizeof(ButtonEncoderEvent_t));
    if (encoder_event_queue == 0) {
        assert(false);
    }

    chip_gpio_on_edges(BUTTON0_ENCODER_PIN1, CHIP_GPIO_EDGE_BOTH, _isr_on_encoder_update);
    chip_gpio_on_edges(BUTTON0_ENCODER_PIN2, CHIP_GPIO_EDGE_BOTH, _isr_on_encoder_update);
    // Both edges of the knob's button: down and up (ADR-010, gate 4)
    chip_gpio_on_edges(BUTTON0_ENC_PIN, CHIP_GPIO_EDGE_BOTH, _isr_on_button_enc_update);
    chip_gpio_on_edges(BUTTON0_RST_PIN, CHIP_GPIO_EDGE_FALL, _isr_on_button_rst_update);

    printf("done\n");
}
