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
#include "ws2812.pio.h"
#include "configuration.h"
#include "eeprom.h"
#include "common.h"
#include "activity.h"
#include "settings.h"


// The parameters for updating the NeoPixel LED colors (including the target LED information)
typedef struct {
    pio_config_t * pio_config;
    uint32_t colour;
} _neopixel_led_colour_update_params_t;


// Global configuration for neopixel LED instance
neopixel_led_config_t neopixel_led_config;
const eeprom_neopixel_led_metadata_t default_neopixel_led_metadata = {
    .neopixel_data_rev = 0,
    .default_led_colours = {
        .led1_colour._raw_colour = RGB_COLOUR_DULL_WHITE,
        .led2_colour._raw_colour = RGB_COLOUR_DULL_WHITE,
        .mini12864_backlight_colour._raw_colour = RGB_COLOUR_WHITE,
    },
    .pwm_out_led_chain_count = NEOPIXEL_LED_CHAIN_COUNT_2,  // 2x LED chains by default
    .pwm_out_led_is_rgbw = true,                            // Default to RGBW
    .pwm_out_led_colour_order = NEOPIXEL_COLOUR_ORDER_RGB,  // Default to RGB colour order
};

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


/*
    The LEDs' settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/leds)
*/
static const setting_choice_t led_colour_order_choices[] = {
    {.held = NEOPIXEL_COLOUR_ORDER_RGB, .text = "rgb", .label = "RGB"},
    {.held = NEOPIXEL_COLOUR_ORDER_GRB, .text = "grb", .label = "GRB"},
};


#define LED_FIELD(member) SETTING_AT(eeprom_neopixel_led_metadata_t, member)
#define LED_COLOUR_APPLIES "now when no mode has the LEDs; otherwise when it gives them back"

static const setting_field_t led_fields[] = {
    {.name = "backlight_colour", .label = "Backlight", .group = "Display board",
     .about = "The display's backlight",
     .type = SETTING_COLOUR, LED_FIELD(default_led_colours.mini12864_backlight_colour),
     .applies = LED_COLOUR_APPLIES},
    {.name = "led1_colour", .label = "LED 1", .group = "Display board",
     .about = "The display board's first LED, by the encoder, when no mode colours it; PWM3's LEDs follow it",
     .type = SETTING_COLOUR, LED_FIELD(default_led_colours.led1_colour), .applies = LED_COLOUR_APPLIES},
    {.name = "led2_colour", .label = "LED 2", .group = "Display board",
     .about = "The display board's second LED, when no mode colours it",
     .type = SETTING_COLOUR, LED_FIELD(default_led_colours.led2_colour), .applies = LED_COLOUR_APPLIES},
    {.name = "pwm3_chain_count", .label = "PWM3 LEDs", .group = "PWM3",
     .about = "How many LEDs are chained on the PWM3 output",
     .type = SETTING_INTEGER, LED_FIELD(pwm_out_led_chain_count), .min = 1.0f, .max = 16.0f, .applies = "now"},
    {.name = "pwm3_colour_order", .label = "PWM3 colour order", .group = "PWM3",
     .about = "The order the PWM3 LEDs take their colours in",
     .type = SETTING_CHOICE, LED_FIELD(pwm_out_led_colour_order), SETTING_CHOICES(led_colour_order_choices),
     .applies = "now"},
    {.name = "pwm3_rgbw", .label = "PWM3 RGBW", .group = "PWM3",
     .about = "The PWM3 LEDs have a white as well as red, green and blue",
     .type = SETTING_BOOLEAN, LED_FIELD(pwm_out_led_is_rgbw), .applies = "at_restart"},
};


static void *led_settings_live(int32_t id) {
    (void) id;
    return &neopixel_led_config.eeprom_neopixel_led_metadata;
}


static void led_settings_apply(const void *before, int32_t id) {
    (void) before;
    (void) id;
    // A mode colours the LEDs while it runs, and paints these back when it gives them up
    if (activity_get() == ACTIVITY_NONE) {
        const neopixel_led_colours_t *colours = &neopixel_led_config.eeprom_neopixel_led_metadata.default_led_colours;
        neopixel_led_set_colour(colours->mini12864_backlight_colour, colours->led1_colour, colours->led2_colour, true);
    }
}


static bool led_settings_is_saved(void) {
    return config_is_saved(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, &neopixel_led_config.eeprom_neopixel_led_metadata,
                           sizeof(eeprom_neopixel_led_metadata_t));
}


static const settings_resource_t led_settings = {
    .path = "/api/v1/settings/leds",
    .save_path = "/api/v1/settings/leds/save",
    .revert_path = "/api/v1/settings/leds/revert",
    .addr = EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR,
    .label = "Lights",
    .about = "The display's backlight and LEDs, and the LEDs on PWM3",
    SETTING_FIELDS(led_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(eeprom_neopixel_led_metadata_t),
    .live = led_settings_live,
    .apply = led_settings_apply,
    .save = neopixel_led_config_save,
    .is_saved = led_settings_is_saved,
};


bool neopixel_led_init(void) {
    bool is_ok = true;
    
    // Initialize configuration
    memset(&neopixel_led_config, 0x0, sizeof(neopixel_led_config));
    is_ok = load_config(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, &neopixel_led_config.eeprom_neopixel_led_metadata, &default_neopixel_led_metadata, sizeof(neopixel_led_config.eeprom_neopixel_led_metadata), EEPROM_NEOPIXEL_LED_METADATA_REV);

    if (!is_ok) {
        printf("Unable to read neopixel configuration\n");
        return is_ok;
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
        printf("Unable to initialize mini12864 Neopixel PIO\n");
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
    settings_register(&led_settings);

    return true;
}


bool neopixel_led_config_save() {
    bool is_ok = save_config(EEPROM_NEOPIXEL_LED_CONFIG_BASE_ADDR, &neopixel_led_config.eeprom_neopixel_led_metadata, sizeof(eeprom_neopixel_led_metadata_t));
    return is_ok;
}


