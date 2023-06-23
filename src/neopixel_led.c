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
#include "neopixel_led.h"
#include "generated/ws2812.pio.h"
#include "configuration.h"
#include "eeprom.h"




typedef struct {
    eeprom_neopixel_led_metadata_t eeprom_neopixel_led_metadata;

    uint32_t current_mini12864_backlight_colour;
    uint32_t current_led1_colour;
    uint32_t current_led2_colour;
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


void neopixel_led_set_colour(uint32_t led1_colour, uint32_t led2_colour, uint32_t mini12864_backlight_colour) {
    // TODO: Implement the queue and task control
}

static void _neopixel_led_set_colour(uint32_t led1_colour, uint32_t led2_colour, uint32_t mini12864_backlight_colour) {
    put_pixel(led1_colour);  // Encoder RGB1
    put_pixel(led2_colour);  // Encoder RGB2
    put_pixel(mini12864_backlight_colour);  // 12864 Backlight
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
        neopixel_led_config.eeprom_neopixel_led_metadata.default_mini12864_backlight_colour = urgb_u32(0xFF, 0xFF, 0xFF);
        neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour1 = urgb_u32(0x0F, 0x0F, 0x0F);
        neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour2 = urgb_u32(0xFF, 0xFF, 0x00);
        neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour1 = urgb_u32(0x0F, 0x0F, 0x0F);
        neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour2 = urgb_u32(0x00, 0xFF, 0xFF);

        // Write data back
        is_ok = neopixel_led_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Initialise current values
    neopixel_led_config.current_mini12864_backlight_colour = neopixel_led_config.eeprom_neopixel_led_metadata.default_mini12864_backlight_colour;
    neopixel_led_config.current_led1_colour = neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour1;
    neopixel_led_config.current_led2_colour = neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour1;


    // Configure Neopixel (WS2812) with PIO
    uint ws2812_sm = pio_claim_unused_sm(pio0, true);
    uint ws2812_offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, ws2812_sm, ws2812_offset, NEOPIXEL_PIN, 800000, false);

    // Set default colour
    _neopixel_led_set_colour(
        neopixel_led_config.current_led1_colour,
        neopixel_led_config.current_led2_colour,
        neopixel_led_config.current_mini12864_backlight_colour
    );

    return true;
}


bool neopixel_led_config_save() {
    bool is_ok = eeprom_write(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, (uint8_t *) &neopixel_led_config.eeprom_neopixel_led_metadata, sizeof(eeprom_neopixel_led_metadata_t));
    return is_ok;
}


uint32_t _to_hex_colour(char * string) {
    uint32_t value = 0;

    if (string) {
        // If the string is an escaped character, then skip the next 3 characters
        if (string[0] == '%') {
            string += 3;
        }

        value = strtol(string, NULL, 16);
    }

    return value;
}


bool http_rest_neopixel_led_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char neopixel_config_json_buffer[128];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "12864bl") == 0) {
            // Remove %23 (#) from the request
            neopixel_led_config.eeprom_neopixel_led_metadata.default_mini12864_backlight_colour = _to_hex_colour(values[idx]);
        }
        else if (strcmp(params[idx], "led1_c1") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour1 = _to_hex_colour(values[idx]);
        }
        else if (strcmp(params[idx], "led1_c2") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour2 = _to_hex_colour(values[idx]);
        }
        else if (strcmp(params[idx], "led2_c1") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour1 = _to_hex_colour(values[idx]);
        }
        else if (strcmp(params[idx], "led2_c2") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour2 = _to_hex_colour(values[idx]);
        }
    }

    // Response
    snprintf(neopixel_config_json_buffer, 
             sizeof(neopixel_config_json_buffer),
             "{\"12864bl\":\"#%06x\",\"led1_c1\":\"#%06x\",\"led1_c2\":\"#%06x\",\"led2_c1\":\"#%06x\",\"led2_c2\":\"#%06x\"}",
             neopixel_led_config.eeprom_neopixel_led_metadata.default_mini12864_backlight_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour1,
             neopixel_led_config.eeprom_neopixel_led_metadata.led1_colour2,
             neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour1,
             neopixel_led_config.eeprom_neopixel_led_metadata.led2_colour2);

    size_t data_length = strlen(neopixel_config_json_buffer);
    file->data = neopixel_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
