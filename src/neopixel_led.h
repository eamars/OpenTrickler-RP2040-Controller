#ifndef NEOPIXEL_LED_H_
#define NEOPIXEL_LED_H_

#include <stdbool.h>
#include <semphr.h>
#include "http_rest.h"
#include "common.h"

#define EEPROM_NEOPIXEL_LED_METADATA_REV                     4              // 16 byte 


// A struct that uses 8bit bitfields to map R, G, B, W into a uint32_t memory space.
#define RGB_COLOUR_GREEN 0x00FF00ul
#define RGB_COLOUR_YELLOW 0xFFFF00ul
#define RGB_COLOUR_RED 0xFF0000ul
#define RGB_COLOUR_BLUE 0x0000FFul
#define RGB_COLOUR_WHITE 0xFFFFFFul
#define RGB_COLOUR_DULL_WHITE 0x0F0F0Ful


typedef union {
    uint32_t _raw_colour;
    struct __attribute__((__packed__)){
        // Byte order to match with hex string from the web portal
        uint8_t b;  // Blue
        uint8_t g;  // Green
        uint8_t r;  // Red
        uint8_t w;  // White
    };
} rgbw_u32_t;


typedef struct {
    rgbw_u32_t led1_colour;
    rgbw_u32_t led2_colour;
    rgbw_u32_t mini12864_backlight_colour;
} neopixel_led_colours_t;

typedef enum {
    NEOPIXEL_LED_CHAIN_COUNT_1 = 1,  // 1 LED from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_2 = 2,  // 2 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_3 = 3,  // 3 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_4 = 4,  // 4 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_5 = 5,  // 5 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_6 = 6,  // 6 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_7 = 7,  // 7 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_8 = 8,  // 8 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_9 = 9,  // 9 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_10 = 10, // 10 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_11 = 11, // 11 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_12 = 12, // 12 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_13 = 13, // 13 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_14 = 14, // 14 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_15 = 15, // 15 LEDs from the same chain
    NEOPIXEL_LED_CHAIN_COUNT_16 = 16, // 16 LEDs from the same chain
} neopixel_led_chain_count_t;

typedef enum {
    NEOPIXEL_COLOUR_ORDER_RGB = 0,  // RGB
    NEOPIXEL_COLOUR_ORDER_GRB = 1, // GRB
} neopixel_colour_order_t;


typedef struct {
    uint16_t neopixel_data_rev;
    neopixel_led_colours_t default_led_colours;

    // PWM OUT LED configurations
    neopixel_led_chain_count_t pwm_out_led_chain_count;
    neopixel_colour_order_t pwm_out_led_colour_order;
    bool pwm_out_led_is_rgbw;   // true: RGBW, false: RGB
} eeprom_neopixel_led_metadata_t;


/* Configuration file for neopixel LEDs on both mini12864 display (including three LEDs in chain) and one dedicated LED on PWM3
   PWM3 output will mirror the LED1 colour from mini12864 display.
*/
typedef struct {
    eeprom_neopixel_led_metadata_t eeprom_neopixel_led_metadata;

    SemaphoreHandle_t mutex;
    TaskHandle_t neopixel_control_task_handler;
    pio_config_t mini12864_pio_config;
    pio_config_t pwm3_pio_config;
} neopixel_led_config_t;


#ifdef __cplusplus
extern "C" {
#endif


bool neopixel_led_init(void);
bool neopixel_led_config_save();
void neopixel_led_set_colour(rgbw_u32_t mini12864_backlight_colour, rgbw_u32_t led1_colour, rgbw_u32_t led2_colour, bool block_wait);
bool http_rest_neopixel_led_config(struct fs_file *file, int num_params, char *params[], char *values[]);

uint32_t hex_string_to_decimal(char * string);

// Low level function to bypass RTOS to configure colour directly
void _neopixel_led_set_colour(uint32_t led1_colour, uint32_t led2_colour, uint32_t mini12864_backlight_colour);

#ifdef __cplusplus
}
#endif

#endif  // NEOPIXEL_LED_H_