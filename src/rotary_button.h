#ifndef ROTARY_BUTTON_H_
#define ROTARY_BUTTON_H_


typedef enum {
    BUTTON_ENCODER_ROTATE_CW = 1 << 0,
    BUTTON_ENCODER_ROTATE_CCW = 1 << 1,
    BUTTON_ENCODER_PRESSED = 1 << 2,
    BUTTON_RST_PRESSED = 1 << 3,
} ButtonEncoderEvent_t;


#endif  // ROTARY_BUTTON_H_