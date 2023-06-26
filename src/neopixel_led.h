#ifndef NEOPIXEL_LED_H_
#define NEOPIXEL_LED_H_

#include <stdbool.h>
#include "http_rest.h"


#define EEPROM_NEOPIXEL_LED_METADATA_REV                     1              // 16 byte 


typedef struct {
    uint16_t neopixel_data_rev;
    uint32_t default_mini12864_backlight_colour;

    uint32_t led1_colour1;
    uint32_t led1_colour2;

    uint32_t led2_colour1;
    uint32_t led2_colour2;
} eeprom_neopixel_led_metadata_t;


typedef enum {
    NEOPIXEL_LED_COLOUR_1,
    NEOPIXEL_LED_COLOUR_2,
    NEOPIXEL_LED_NO_CHANGE,
} neopixel_led_colour_t;


#ifdef __cplusplus
extern "C" {
#endif


bool neopixel_led_init(void);
bool neopixel_led_config_save();
void neopixel_led_set_colour(neopixel_led_colour_t led1_colour, neopixel_led_colour_t led2_colour, bool block_wait);
bool http_rest_neopixel_led_config(struct fs_file *file, int num_params, char *params[], char *values[]);

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);


#ifdef __cplusplus
}
#endif

#endif  // NEOPIXEL_LED_H_