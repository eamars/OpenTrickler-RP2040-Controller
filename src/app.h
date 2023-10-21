#ifndef APP_H_
#define APP_H_

#include "eeprom.h"

#define EEPROM_APP_CONFIG_DATA_REV              1           // 16 byte

typedef enum {
    APP_STATE_DEFAULT = 0,
    APP_STATE_ENTER_CHARGE_MODE = 1,
    // 2 is removed
    // 3 is removed
    // 4 is removed
    APP_STATE_ENTER_CLEANUP_MODE = 5,
    APP_STATE_ENTER_SCALE_CALIBRATION = 6,
    APP_STATE_ENTER_EEPROM_SAVE = 7,
    APP_STATE_ENTER_EEPROM_ERASE = 8,
    APP_STATE_ENTER_REBOOT = 9,
    APP_STATE_ENTER_WIFI_INFO = 10,
    APP_STATE_ENTER_BOOTLOADER = 11,
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
uint8_t software_reboot();
uint8_t reboot_to_bootloader();
bool http_app_config();

#ifdef __cplusplus
}
#endif

#endif  // APP_H_
