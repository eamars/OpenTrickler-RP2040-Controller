#include <stdint.h>
#include "eeprom.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"


uint8_t eeprom_save_all() {
    scale_config_save();
    motor_config_save();
    charge_mode_config_save();
    return 30;  // Configuration Menu ID
}

