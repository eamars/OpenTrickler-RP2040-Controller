#ifndef APP_H_
#define APP_H_

#include "eeprom.h"

#define EEPROM_APP_CONFIG_DATA_REV              1           // 16 byte

typedef enum {
    APP_STATE_DEFAULT = 0,
    // Start or Cleanup pressed with a motor driver not OK (ADR-012, step 6). Set from code, never by a form's exit
    // button, so it is numbered apart from the states the forms name by number. MUI writes one byte: below 256
    APP_STATE_VIEW_MOTOR_FAULT = 100,
    APP_STATE_ENTER_CHARGE_MODE = 1,
    // 2 is removed: a charge from the web (ADR-003, gate 4: a start request to the menu, activity.h)
    // 3 is removed
    // 4 is removed
    APP_STATE_ENTER_CLEANUP_MODE = 5,
    // 6 is removed: the A&D's calibration, which no scale supports (the owner, ADR-010)
    APP_STATE_ENTER_EEPROM_SAVE = 7,
    APP_STATE_ENTER_EEPROM_ERASE = 8,
    APP_STATE_ENTER_REBOOT = 9,
    APP_STATE_ENTER_WIFI_INFO = 10,
    // A firmware update asked by the API (ADR-004): set from code, numbered apart as the motor fault's view is
    APP_STATE_ENTER_UPDATE = 101,
} AppState_t;


typedef struct {
    
} app_persistent_config_t;


typedef struct {
    app_persistent_config_t persistent_config;
} app_config_t;


#ifdef __cplusplus
extern "C" {
#endif

bool app_init();
bool http_app_config();

#ifdef __cplusplus
}
#endif

#endif  // APP_H_
