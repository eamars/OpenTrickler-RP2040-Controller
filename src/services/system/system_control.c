#include <string.h>

#include "chip.h"
#include "system_control.h"

// What ends before a restart, in the order handed over
#define SYSTEM_BEFORE_RESTART_MAX 4
static void (*system_restart_ends[SYSTEM_BEFORE_RESTART_MAX])(void);
static size_t system_restart_end_count = 0;


bool system_before_restart(void (*end)(void)) {
    if (system_restart_end_count >= SYSTEM_BEFORE_RESTART_MAX) {
        return false;
    }
    system_restart_ends[system_restart_end_count] = end;
    system_restart_end_count += 1;
    return true;
}


void system_prepare_restart(void) {
    for (size_t idx = 0; idx < system_restart_end_count; idx += 1) {
        system_restart_ends[idx]();
    }
}


int software_reboot() {
    system_prepare_restart();
    chip_restart(0);

    return 0;
}


const char *system_board_name(void) {
#if PICO_RP2350
    return "pico2_w";
#else
    return "pico_w";
#endif
}


