#include <string.h>

#include "hardware/watchdog.h"

#include "system_control.h"
#include "http_server.h"
#include "common.h"
#include "eeprom.h"
#include "version.h"


int software_reboot() {
    http_server_close_all(1000);    // each client told the unit has gone, not left with a request that hangs
    watchdog_reboot(0, 0, 0);

    return 0;
}


