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
#include <semphr.h>
#include <string.h>

#include "neopixel_led.h"
#include "generated/ws2812.pio.h"
#include "configuration.h"
#include "eeprom.h"
#include "common.h"



// The parameters for updating the NeoPixel LED colors (including the target LED information)
typedef struct {
    pio_config_t * pio_config;
    uint32_t colour;
} _neopixel_led_colour_update_params_t;


// Global configuration for neopixel LED instance
neopixel_led_config_t neopixel_led_config;

uint32_t urgbw_u32(rgbw_u32_t colour, neopixel_colour_order_t colour_order) {

    uint32_t output;

    if (colour_order == NEOPIXEL_COLOUR_ORDER_GRB) {
        output = ((uint32_t) (colour.g) << 8) |
                 ((uint32_t) (colour.r) << 16) |
                 ((uint32_t) (colour.b)) |
                 ((uint32_t) (colour.w) << 24);
    } else if (colour_order == NEOPIXEL_COLOUR_ORDER_RGB) {
        output = ((uint32_t) (colour.r) << 8) |
                 ((uint32_t) (colour.g) << 16) |
                 ((uint32_t) (colour.b)) |
                 ((uint32_t) (colour.w) << 24);
    }
    
    return output;
}
 
static inline void put_pixel(pio_config_t * pio_config, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio_config->pio, pio_config->sm, pixel_grb << 8u);
}


// Low level function to bypass RTOS to configure colour directly
void _neopixel_led_set_colour(uint32_t led1_colour, uint32_t led2_colour, uint32_t mini12864_backlight_colour) {
    put_pixel(&neopixel_led_config.mini12864_pio_config, led1_colour);  // Encoder RGB1
    put_pixel(&neopixel_led_config.mini12864_pio_config, led2_colour);  // Encoder RGB2
    put_pixel(&neopixel_led_config.mini12864_pio_config, mini12864_backlight_colour);  // 12864 Backlight
}

void _neopixel_pwm_out_set_colour(uint32_t new_colour) {
    for (int i = 0; i < neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_chain_count; i++) {
        put_pixel(&neopixel_led_config.pwm3_pio_config, new_colour);
    }
}


void neopixel_led_set_colour(rgbw_u32_t mini12864_backlight_colour, rgbw_u32_t led1_colour, rgbw_u32_t led2_colour, bool block_wait) {
    // Assign delay time
    TickType_t ticks_to_wait = 0;
    if (block_wait) {
        ticks_to_wait = portMAX_DELAY;
    }
    // Wait for the mutex
    if (xSemaphoreTake(neopixel_led_config.mutex, ticks_to_wait) != pdTRUE) {
        return;  // unable to take mutex, return immediately
    }

    /* Important: do not change the order of LED update operations. 
       Neopixel LEDs on the same string are updated in the order they are sent. The update order has to be: 
        1. Encoder RGB1
        2. Encoder RGB2
        3. 12864 Backlight
    */
    _neopixel_led_set_colour(
        urgbw_u32(led1_colour, NEOPIXEL_COLOUR_ORDER_GRB),
        urgbw_u32(led2_colour, NEOPIXEL_COLOUR_ORDER_GRB),
        urgbw_u32(mini12864_backlight_colour, NEOPIXEL_COLOUR_ORDER_GRB)
    );

    /* Update PWM3 output to mirror new_led1_colour */
    // Infuse white intensity
    _neopixel_pwm_out_set_colour(
        urgbw_u32(led1_colour, neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_colour_order)
    );

    // Release resource
    xSemaphoreGive(neopixel_led_config.mutex);
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
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour._raw_colour = RGB_COLOUR_DULL_WHITE;
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour._raw_colour = RGB_COLOUR_DULL_WHITE;
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour._raw_colour = RGB_COLOUR_WHITE;

        // Default to 1 LED chain
        neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_chain_count = NEOPIXEL_LED_CHAIN_COUNT_2;

        // Default to RGBW
        neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_is_rgbw = true;

        // Default to RGB colour order
        neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_colour_order = NEOPIXEL_COLOUR_ORDER_RGB;

        // Write data back
        is_ok = neopixel_led_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Initialize the mutex
    neopixel_led_config.mutex = xSemaphoreCreateMutex();
    if (neopixel_led_config.mutex == NULL) {
        printf("Unable to create neopixel LED mutex\n");
        return false;
    }

    // Configure Neopixel for mini12864 display
    PIO pio;
    uint sm;
    uint offset;

    is_ok = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, NEOPIXEL_PIN, 1, true
    );
    if (!is_ok) {
        printf("Unable to initialize mini12864 Neopixel PIO");
        return false;
    }

    ws2812_program_init(pio, sm, offset, NEOPIXEL_PIN, 800000, false);

    // Save pio and sm for later access
    neopixel_led_config.mini12864_pio_config.pio = pio;
    neopixel_led_config.mini12864_pio_config.sm = sm;

    // Set default colour
    _neopixel_led_set_colour(
        urgbw_u32(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour, NEOPIXEL_COLOUR_ORDER_GRB),
        urgbw_u32(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour, NEOPIXEL_COLOUR_ORDER_GRB),
        urgbw_u32(neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour, NEOPIXEL_COLOUR_ORDER_GRB)
    );

    // Configure Neopixel for PWM3
    is_ok = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, NEOPIXEL_PWM3_PIN, 1, true
    );
    if (!is_ok) {
        printf("Unable to initialize PWM3 Neopixel PIO");
        return false;
    }

    ws2812_program_init(pio, sm, offset, NEOPIXEL_PWM3_PIN, 800000, neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_is_rgbw);
    
    // Save pio and sm for later access
    neopixel_led_config.pwm3_pio_config.pio = pio;
    neopixel_led_config.pwm3_pio_config.sm = sm;

    // Set default colour for PWM3
    // Infuse white intensity
    rgbw_u32_t colour = neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour;
    _neopixel_pwm_out_set_colour(
        urgbw_u32(colour, neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_colour_order)
    );

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
    // l3 (str): PWM OUT led chain count
    // l4 (bool): RGBW: true, RGB: false
    // l5 (int): PWM OUT led colour order
    // l6 (int): PWM OUT white intensity
    // ee (bool): save to eeprom

    static char neopixel_config_json_buffer[256];
    bool save_to_eeprom = false;

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "bl") == 0) {
            // Remove %23 (#) from the request
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "l1") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "l2") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour._raw_colour = hex_string_to_decimal(values[idx]);
        }
        else if (strcmp(params[idx], "l3") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_chain_count = (neopixel_led_chain_count_t) atoi(values[idx]);
        }
        else if (strcmp(params[idx], "l4") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_is_rgbw = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "l5") == 0) {
            neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_colour_order = (neopixel_colour_order_t) atoi(values[idx]);
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
             "%s"
             "{\"bl\":\"#%06lx\",\"l1\":\"#%06lx\",\"l2\":\"#%06lx\",\"l3\":%d,\"l4\":%s,\"l5\":%d}",
             http_json_header,
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour._raw_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour._raw_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour._raw_colour,
             neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_chain_count,
             boolean_to_string(neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_is_rgbw),
             neopixel_led_config.eeprom_neopixel_led_metadata.pwm_out_led_colour_order
    );
    size_t data_length = strlen(neopixel_config_json_buffer);
    file->data = neopixel_config_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    // Update new colour
    neopixel_led_set_colour(
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.mini12864_backlight_colour,
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led1_colour,
        neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours.led2_colour,
        true  // block wait
    );

    return true;
}
