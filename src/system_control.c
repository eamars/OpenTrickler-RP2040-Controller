#include <string.h>

#include "hardware/watchdog.h"

#include "system_control.h"
#include "common.h"
#include "eeprom.h"
#include "version.h"


int software_reboot() {
    watchdog_reboot(0, 0, 0);

    return 0;
}


