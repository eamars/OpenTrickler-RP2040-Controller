#ifndef APP_H_
#define APP_H_

typedef enum {
    APP_STATE_DEFAULT = 0,
    APP_STATE_ENTER_CHARGE_MODE = 1,
    // 2 is removed
    APP_STATE_ENTER_ACCESS_POINT_MODE = 3,
    // 4 is removed
    APP_STATE_ENTER_CLEANUP_MODE = 5,
    APP_STATE_ENTER_SCALE_CALIBRATION = 6,
    APP_STATE_ENTER_EEPROM_SAVE = 7,
    APP_STATE_ENTER_EEPROM_ERASE = 8,
} AppState_t;



#endif  // APP_H_