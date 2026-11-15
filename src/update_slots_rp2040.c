/*
 * The image slots on the RP2040 (ADR-004). Until its bootloader comes (gate 1, the Pico W's step), the Pico W
 * keeps one image at the start of flash: no slots, no trial, nothing to try.
 */
#include "update_slots.h"

#if PICO_RP2040

#define UPDATE_SLOTS_NONE (-1)


bool update_slots_read(update_slots_t *slots) {
    slots->running = -1;
    slots->on_trial = false;
    slots->free_offset = 0;
    slots->free_size = 0;
    return false;
}


int update_slots_try_other(void) {
    return UPDATE_SLOTS_NONE;
}


int update_slots_confirm(void) {
    return UPDATE_SLOTS_NONE;
}

#endif  // PICO_RP2040
