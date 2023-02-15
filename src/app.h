#ifndef APP_H_
#define APP_H_

typedef enum {
    APP_STATE_DEFAULT = 0,
    APP_STATE_ENTER_CHARGE_MODE = 1,
    APP_STATE_ENTER_MENU_READY_PAGE = 2,
} AppState_t;


typedef enum {
    UNIT_GRAIN,
    UNIT_GRAM,
} MeasurementUnit_t;


typedef enum {
    USE_TMC2209,
    USE_TMC2130,
    USE_TMC5160,
} MotorControllerSelect_t;


#endif  // APP_H_