#ifndef MINI_12864_MODULE_H_
#define MINI_12864_MODULE_H_


#define EEPROM_MINI_12864_MODULE_DATA_REV           2

#include "http_rest.h"


typedef enum {
    BUTTON_NO_EVENT = 0,
    BUTTON_ENCODER_ROTATE_CW,
    BUTTON_ENCODER_ROTATE_CCW,
    BUTTON_ENCODER_PRESSED,
    BUTTON_RST_PRESSED,

    // Overrides from other sources, used to signal other thread to proceed
    OVERRIDE_FROM_REST,
} ButtonEncoderEvent_t;

typedef enum {
    DISPLAY_ROTATION_0 = 0,
    DISPLAY_ROTATION_90 = 1,
    DISPLAY_ROTATION_180 = 2,
    DISPLAY_ROTATION_270 = 3,
} display_rotation_t;


typedef struct {
    uint32_t data_rev;
    bool inverted_encoder_direction;
    display_rotation_t display_rotation;
} mini_12864_module_config_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Wait for button encoder input. 
*/
ButtonEncoderEvent_t button_wait_for_input(bool block);
bool mini_12864_module_init(void);
void button_init(void);
void display_init(void);
bool http_rest_button_control(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_mini_12864_module_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool button_config_save();

#ifdef __cplusplus
}
#endif



#endif  // MINI_12864_MODULE_H_