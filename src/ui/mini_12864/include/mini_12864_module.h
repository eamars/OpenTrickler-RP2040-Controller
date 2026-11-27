#ifndef MINI_12864_MODULE_H_
#define MINI_12864_MODULE_H_

#include <stdbool.h>
#include <stdint.h>




typedef enum {
    BUTTON_NO_EVENT = 0,
    BUTTON_ENCODER_ROTATE_CW,
    BUTTON_ENCODER_ROTATE_CCW,
    BUTTON_ENCODER_PRESSED,     // a whole click, as the remote control's `press enc` sends it
    BUTTON_RST_PRESSED,

    // Not presses: the menu woken to look at a start asked from outside it, or a job's end (menu_wake()), and the
    // remote control's `screen <id>` (menu_goto_screen(); ADR-001)
    MENU_WAKE,
    MENU_REMOTE_SCREEN,
} ButtonEncoderEvent_t;

typedef enum : uint8_t {
    DISPLAY_ROTATION_0 = 0,
    DISPLAY_ROTATION_90 = 1,
    DISPLAY_ROTATION_180 = 2,
    DISPLAY_ROTATION_270 = 3,
} display_rotation_t;


// What the charge's screen leads with (ADR-010, gate 6)
typedef enum : uint8_t {
    DISPLAY_CHARGE_VIEW_WEIGHT = 0,
    DISPLAY_CHARGE_VIEW_CHART = 1,
} display_charge_view_t;

typedef struct {
    uint32_t data_rev;
    bool inverted_encoder_direction;
    display_rotation_t display_rotation;
    bool inverted_display;
    display_charge_view_t charge_view;  // in the struct's last byte of filler (ADR-010, gate 6)
} mini_12864_module_config_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Wait for button encoder input for at most this long: BUTTON_NO_EVENT when nothing came.
*/
ButtonEncoderEvent_t button_wait_for_input_ms(uint32_t timeout_ms);

/**
 * An event put in the knob's queue as a press or a turn would be, waiting at most this long for room: the menu's own
 * wakes and the console's presses. False when there was no room, or no queue yet.
*/
bool button_send(ButtonEncoderEvent_t event, uint32_t timeout_ms);

// Whether the screen is stored inverted (its settings, mini_12864_settings.c)
bool mini_12864_inverted_display(void);


/**
 * The knob's tap (ADR-010): what its interrupts saw, newest last, kept until read. Kinds: 'd' and 'u' the button's
 * down (a press, sent) and up taken, 'X' and 'x' an edge of the button dropped as a bounce (the pin then low or
 * high), 'r' and 'l' a turn sent (clockwise, anticlockwise as wired), 'q' an event the queue had no room for.
 */
typedef struct {
    uint32_t at_us;
    char kind;
} knob_tap_entry_t;

/**
 * Take the tap's entries and clear it.
 *
 * @param out room for max entries.
 * @param max its size.
 * @param lost set to the entries that did not fit in the tap since it was last read.
 * @return the entries written.
 */
size_t mini_12864_knob_tap(knob_tap_entry_t *out, size_t max, uint32_t *lost);
bool mini_12864_module_init(void);
void button_init(void);
bool button_config_save();
bool mini_12864_module_config_save(void);
// Its settings' table, for the API and the console (mini_12864_settings.c; ADR-003, gate 3)
bool mini_12864_settings_register(void);

#ifdef __cplusplus
}
#endif



#endif  // MINI_12864_MODULE_H_