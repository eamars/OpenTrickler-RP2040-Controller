#include <stdint.h>
#include "eeprom.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "hardware/watchdog.h"


extern bool cat24c256_eeprom_erase();


uint8_t eeprom_save_all() {
    scale_config_save();
    motor_config_save();
    charge_mode_config_save();
    return 37;  // Configuration Menu ID
}

uint8_t eeprom_erase(bool reboot) {
    cat24c256_eeprom_erase();

    if (reboot) {
        watchdog_reboot(0, 0, 0);
    }
    
    return 37;  // Configuration Menu ID
}