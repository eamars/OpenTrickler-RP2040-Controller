#ifndef ROTARY_BUTTON_H_
#define ROTARY_BUTTON_H_


#define EEPROM_ROTARY_BUTTON_DATA_REV           1

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


typedef struct {
    uint32_t rotary_button_data_rev;
    bool inverted_encoder_direction;

} rotary_button_config_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Wait for button encoder input. 
*/
ButtonEncoderEvent_t button_wait_for_input(bool block);
bool button_init();
bool http_rest_button_control(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_button_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool button_config_save();

#ifdef __cplusplus
}
#endif



#endif  // ROTARY_BUTTON_H_