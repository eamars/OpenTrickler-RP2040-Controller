#ifndef DISPLAY_CONFIG_H_
#define DISPLAY_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "http_rest.h"

#define EEPROM_DISPLAY_CONFIG_BASE_ADDR     13 * 1024       // 13K - Display type selection
#define EEPROM_DISPLAY_CONFIG_DATA_REV      2

typedef enum {
    DISPLAY_TYPE_MINI_12864 = 0,    // Default: Mini 12864 monochrome
    DISPLAY_TYPE_TFT35 = 1,         // TFT35 V3.0.1: 3.5 inch, 480x320
    DISPLAY_TYPE_TFT43 = 2,         // TFT43 V3.0: 4.3 inch, 480x272
} display_type_t;

typedef enum {
    DISPLAY_ROTATION_0 = 0,
    DISPLAY_ROTATION_90 = 1,
    DISPLAY_ROTATION_180 = 2,
    DISPLAY_ROTATION_270 = 3,
} display_rotation_t;

typedef struct {
    uint32_t data_rev;
    display_type_t display_type;
    display_rotation_t rotation;
    uint8_t brightness;             // 0-255, for TFT displays
    bool inverted_encoder;          // For Mini 12864 rotary encoder
} display_config_t;

#ifdef __cplusplus
extern "C" {
#endif

bool display_config_init(void);
bool display_config_save(void);
display_type_t display_config_get_type(void);
void display_config_set_type(display_type_t type);
display_config_t* display_config_get(void);
bool http_rest_display_config(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif

#endif  // DISPLAY_CONFIG_H_
