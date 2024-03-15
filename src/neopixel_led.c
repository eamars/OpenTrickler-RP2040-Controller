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
//        -- |10  9 | --
//   (RESET) | 8  7 | --
//   (MOSI)  | 6  5#| (EN2)
//        -- | 4  3 | (EN1)
//  (LCD_SCK)| 2  1 | --
//            ------
//             EXP2
// 
// For Pico W
// EXP1_6 (Neopixel) <-> PIN17 (GP13)
// 
#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

#include "neopixel_led.h"
#include "generated/ws2812.pio.h"
#include "configuration.h"
#include "eeprom.h"
#include "common.h"


typedef struct {
    eeprom_neopixel_led_metadata_t eeprom_neopixel_led_metadata;

    neopixel_led_colours_t current_led_colours;
    xQueueHandle colour_update_queue;
    TaskHandle_t neopixel_control_task_handler;
} neopixel_led_config_t;


static neopixel_led_config_t neopixel_led_config;


uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (g) << 8) |
            ((uint32_t) (r) << 16) |
            (uint32_t) (b);
}
 
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}


void neopixel_led_set_colour(uint32_t mini12864_backlight_colour, uint32_t led1_colour, uint32_t led2_colour, bool block_wait) {
    neopixel_led_colours_t new_colours;

    if (mini12864_backlight_colour == NEOPIXEL_LED_NO_CHANGE) {
        new_colours.mini12864_backlight_colour = neopixel_led_config.current_led_colours.mini12864_backlight_colour;
    }
    else if (mini12864_backlight_colour == NEOPIXEL_LED_DEFAULT_COLOUR) {
        new_colours.mini12864_backlight_colour = neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour;
    }
    else {
        new_colours.mini12864_backlight_colour = mini12864_backlight_colour;
    }

    if (led1_colour == NEOPIXEL_LED_NO_CHANGE) {
        new_colours.led1_colour = neopixel_led_config.current_led_colours.led1_colour;

    }
    else if (led1_colour == NEOPIXEL_LED_DEFAULT_COLOUR) {
        new_colours.led1_colour = neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour;
    }
    else {
        new_colours.led1_colour = led1_colour;
    }

    if (led2_colour == NEOPIXEL_LED_NO_CHANGE) {
        new_colours.led2_colour = neopixel_led_config.current_led_colours.led2_colour;
    }
    else if (led1_colour == NEOPIXEL_LED_DEFAULT_COLOUR) {
        new_colours.led2_colour = neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour;
    }
    else {
        new_colours.led2_colour = led2_colour;
    }

    TickType_t ticks_to_wait = 0;
    if (block_wait) {
        ticks_to_wait = portMAX_DELAY;
    }

    xQueueSend(neopixel_led_config.colour_update_queue, &new_colours, ticks_to_wait);
}


static void _neopixel_led_set_colour(uint32_t led1_colour, uint32_t led2_colour, uint32_t mini12864_backlight_colour) {
    put_pixel(led1_colour);  // Encoder RGB1
    put_pixel(led2_colour);  // Encoder RGB2
    put_pixel(mini12864_backlight_colour);  // 12864 Backlight
}


void neopixel_control_task(void *p) {
    while (true) {
        neopixel_led_colours_t new_colours;
        xQueueReceive(neopixel_led_config.colour_update_queue, &new_colours, portMAX_DELAY);

        // Apply
        _neopixel_led_set_colour(
            new_colours.led1_colour,
            new_colours.led2_colour,
            new_colours.mini12864_backlight_colour
        );
    }
}


bool neopixel_led_init(void) {
    bool is_ok = true;
    
    // Initialize configuration
    memset(&neopixel_led_config, 0x0, sizeof(neopixel_led_config));

    is_ok = eeprom_read(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, (uint8_t *) &neopixel_led_config.eeprom_neopixel_led_metadata, sizeof(eeprom_neopixel_led_metadata_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR);
        return false;
    }

    // If the revision doesn't match then re-initialize the config
    if (neopixel_led_config.eeprom_neopixel_led_metadata.neopixel_data_rev != EEPROM_NEOPIXEL_LED_METADATA_REV) {
        neopixel_led_config.eeprom_neopixel_led_metadata.neopixel_data_rev = EEPROM_NEOPIXEL_LED_METADATA_REV;

        // Default to white
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour = urgb_u32(0x0F, 0x0F, 0x0F);
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour = urgb_u32(0x0F, 0x0F, 0x0F);
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour = urgb_u32(0xFF, 0xFF, 0xFF);

        // Write data back
        is_ok = neopixel_led_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Initialise current values
    memcpy(&neopixel_led_config.current_led_colours, &neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours, sizeof(neopixel_led_colours_t));

    // Initialize the queue
    neopixel_led_config.colour_update_queue = xQueueCreate(2, sizeof(neopixel_led_colours_t));
    assert(neopixel_led_config.colour_update_queue);

    // Configure Neopixel (WS2812) with PIO
    uint ws2812_sm = pio_claim_unused_sm(pio0, true);
    uint ws2812_offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, ws2812_sm, ws2812_offset, NEOPIXEL_PIN, 800000, false);

    // Set default colour
    _neopixel_led_set_colour(
        neopixel_led_config.current_led_colours.led1_colour,
        neopixel_led_config.current_led_colours.led2_colour,
        neopixel_led_config.current_led_colours.mini12864_backlight_colour
    );

    // Initialize the task
    xTaskCreate(neopixel_control_task, 
                "Neopixel Controller", 
                configMINIMAL_STACK_SIZE, 
                NULL, 
                2, 
                &neopixel_led_config.neopixel_control_task_handler);

    // Register to eeprom save all
    eeprom_register_handler(neopixel_led_config_save);

    return true;
}


bool neopixel_led_config_save() {
    bool is_ok = eeprom_write(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, (uint8_t *) &neopixel_led_config.eeprom_neopixel_led_metadata, sizeof(eeprom_neopixel_led_metadata_t));
    return is_ok;
}


uint32_t hex_string_to_decimal(char * string) {
    uint32_t value = 0;

    // Valid hex decimal starts with #
    // For example, #ff00ff
    if (string && string[0] == '#') {
        value = strtol(string + 1, NULL, 16);
    }

    return value;
}


bool http_rest_neopixel_led_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings：
    // bl (str): mini12864_backlight_colour
    // l1 (str): led1_colour
    // l2 (str): led2_colour
    // ee (bool): save to eeprom

    static char neopixel_config_json_buffer[128];
    bool save_to_eeprom = false;

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "bl") == 0) {
            // Remove %23 (#) from the request
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour= hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "l1") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "l2") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        neopixel_led_config_save();
    }

    // Response
    snprintf(neopixel_config_json_buffer, 
             sizeof(neopixel_config_json_buffer),
             "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
             "{\"bl\":\"#%06x\",\"l1\":\"#%06x\",\"l2\":\"#%06x\"}",
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour);

    size_t data_length = strlen(neopixel_config_json_buffer);
    file->data = neopixel_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
