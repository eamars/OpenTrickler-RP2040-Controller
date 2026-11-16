#ifndef MINI_12864_MODULE_H_
#define MINI_12864_MODULE_H_


#define EEPROM_MINI_12864_MODULE_DATA_REV           2



typedef enum {
    BUTTON_NO_EVENT = 0,
    BUTTON_ENCODER_ROTATE_CW,
    BUTTON_ENCODER_ROTATE_CCW,
    BUTTON_ENCODER_PRESSED,     // a whole click, as the remote control's `key enter` sends it
    BUTTON_RST_PRESSED,

    // Not presses: the menu woken to look at a start asked from outside it, or a job's end (menu_wake()), and the
    // remote control's `screen <id>` (menu_goto_screen(); ADR-001)
    MENU_WAKE,
    MENU_REMOTE_SCREEN,

    // The knob's button down and up (ADR-010, gate 4): the menu tells a click from a hold by the time between
    BUTTON_ENCODER_DOWN,
    BUTTON_ENCODER_UP,
    // The remote control's `press hold`: down with no pin behind it, so the menu does not read the pin before Back
    BUTTON_ENCODER_DOWN_REMOTE,
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

/**
 * Wait for button encoder input for at most this long: BUTTON_NO_EVENT when nothing came.
*/
ButtonEncoderEvent_t button_wait_for_input_ms(uint32_t timeout_ms);

/**
 * The knob's button down now, read from its pin (ADR-010, gate 4): the menu reads it before a hold counts as Back.
*/
bool button_enc_is_down(void);
bool mini_12864_module_init(void);
void button_init(void);
void display_init(void);
bool button_config_save();
bool mini_12864_module_config_save(void);
// Its settings' table, for the API and the console (mini_12864_settings.c; ADR-003, gate 3)
bool mini_12864_settings_register(void);

#ifdef __cplusplus
}
#endif



#endif  // MINI_12864_MODULE_H_