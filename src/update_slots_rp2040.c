/*
 * The image slots on the RP2040 (ADR-004): our bootloader's (bootloader/bootloader.c, src/update_boot.h). An image
 * runs only where it was linked, so it knows its slot from its link (APP_SLOT_INDEX), and it is on trial when its
 * own mark is still set: the bootloader boots a marked image only on a trial. A trial is asked through the watchdog's
 * scratch registers; an image confirms itself by clearing its mark in its last metadata block, a write that only
 * clears bits, so nothing is erased.
 */
#include "update_slots.h"

#if PICO_RP2040

#include <string.h>

#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"

#include "picobin_image.h"
#include "update_boot.h"

#define UPDATE_SLOTS_TRY_DELAY_MS 100   // the console's reply gets out first


static int update_slots_own(picobin_image_t *image) {
    return picobin_image_read(XIP_BASE + update_boot_slot_offset(APP_SLOT_INDEX), UPDATE_SLOT_SIZE, image);
}


bool update_slots_read(update_slots_t *slots) {
    picobin_image_t own;
    int free = 1 - APP_SLOT_INDEX;
    slots->running = APP_SLOT_INDEX;
    slots->on_trial = update_slots_own(&own) && own.trial;
    slots->free_offset = update_boot_slot_offset(free);
    slots->free_size = UPDATE_SLOT_SIZE;
    slots->free_read_at = XIP_BASE + slots->free_offset;
    slots->free_runs_at = XIP_BASE + slots->free_offset;
    return own.valid;
}


bool update_slots_first_fits(const uint8_t *first, uint32_t bytes) {
    if (bytes < UPDATE_BOOT_VECTORS_AT + 8) {
        return false;
    }
    return update_boot_vectors_fit((const uint32_t *) (first + UPDATE_BOOT_VECTORS_AT),
                                   XIP_BASE + update_boot_slot_offset(1 - APP_SLOT_INDEX));
}


int update_slots_try_other(void) {
    uint32_t asked = UPDATE_BOOT_TRY_MAGIC | (uint32_t) (1 - APP_SLOT_INDEX);
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_TRY] = asked;
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_CHECK] = ~asked;
    watchdog_reboot(0, 0, UPDATE_SLOTS_TRY_DELAY_MS);
    while (true) {
        tight_loop_contents();
    }
}


typedef struct {
    uint32_t page;                      // the page's offset in flash
    uint8_t bytes[FLASH_PAGE_SIZE];     // 0xff but for the word that changes: 0xff leaves a bit as it is
} update_slots_page_t;


static void update_slots_program(void *param) {
    const update_slots_page_t *page = param;
    flash_range_program(page->page, page->bytes, FLASH_PAGE_SIZE);
}


int update_slots_confirm(void) {
    picobin_image_t own;
    if (!update_slots_own(&own) || own.image_type == NULL) {
        return PICO_ERROR_NOT_FOUND;
    }
    if (!own.trial) {
        return 0;
    }
    static update_slots_page_t page;
    uint32_t offset = (uint32_t) ((uintptr_t) own.image_type - XIP_BASE);
    page.page = offset & ~(FLASH_PAGE_SIZE - 1u);
    memset(page.bytes, 0xff, sizeof(page.bytes));
    uint32_t cleared = *own.image_type & ~((uint32_t) PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS << 16);
    memcpy(&page.bytes[offset - page.page], &cleared, sizeof(cleared));
    // Flash is written with the other core held off it
    int result = flash_safe_execute(update_slots_program, &page, UINT32_MAX);
    if (result != PICO_OK) {
        return result;
    }
    return (update_slots_own(&own) && !own.trial) ? 0 : PICO_ERROR_GENERIC;
}


void update_slots_note_write(uint32_t first, uint32_t second) {
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_NOTE] = first;
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_NOTE + 1] = second;
}


void update_slots_note_read(uint32_t *first, uint32_t *second) {
    *first = watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_NOTE];
    *second = watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_NOTE + 1];
}

#endif  // PICO_RP2040
