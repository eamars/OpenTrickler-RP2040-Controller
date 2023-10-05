#ifndef NEOPIXEL_LED_H_
#define NEOPIXEL_LED_H_

#include <stdbool.h>
#include "http_rest.h"


#define EEPROM_NEOPIXEL_LED_METADATA_REV                     2              // 16 byte 

#define NEOPIXEL_LED_NO_CHANGE      (uint32_t)(1 << 24)                     // High bit is not used for Neopixel RGB
#define NEOPIXEL_LED_DEFAULT_COLOUR (uint32_t)(1 << 25)


typedef struct {
    uint32_t led1_colour;
    uint32_t led2_colour;
    uint32_t mini12864_backlight_colour;
} neopixel_led_colours_t;


typedef struct {
    uint16_t neopixel_data_rev;
    neopixel_led_colours_t default_led_colours;
} eeprom_neopixel_led_metadata_t;


#ifdef __cplusplus
extern "C" {
#endif


bool neopixel_led_init(void);
bool neopixel_led_config_save();
void neopixel_led_set_colour(uint32_t mini12864_backlight_colour, uint32_t led1_colour, uint32_t led2_colour, bool block_wait);
bool http_rest_neopixel_led_config(struct fs_file *file, int num_params, char *params[], char *values[]);

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
uint32_t hex_string_to_decimal(char * string);


#ifdef __cplusplus
}
#endif

#endif  // NEOPIXEL_LED_H_