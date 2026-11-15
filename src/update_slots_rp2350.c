/*
 * The image slots on the RP2350: its ROM's (ADR-004). The partition table names slot A and slot B
 * (targets/pico2_w_partition_table.json). The ROM boots the better of their two images, maps its slot to the start
 * of flash, and boots an image marked try-before-you-buy only on a restart that asks for it, once, until the image
 * confirms itself (rom_explicit_buy).
 */
#include "update_slots.h"

#if PICO_RP2350

#include <stdio.h>

#include <FreeRTOS.h>

#include "boot/bootrom_constants.h"
#include "boot/picobin.h"
#include "boot/picoboot_constants.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/bootrom.h"

// rom_load_partition_table()'s and rom_explicit_buy()'s work areas, taken from the heap only while they run
#define UPDATE_SLOTS_LOAD_BYTES 0x1000
#define UPDATE_SLOTS_BUY_BYTES 0x1000
#define UPDATE_SLOTS_TRY_DELAY_MS 100   // the console's reply gets out first


// A partition's offset and size in flash
static bool update_slots_location(uint8_t partition, uint32_t *offset, uint32_t *size) {
    uint32_t info[3];
    uint32_t asked = PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | ((uint32_t) partition << 24);
    int words = rom_get_partition_table_info(info, 3, asked);
    if (words == PICO_ERROR_NO_DATA) {
        // Not kept since this start (a restart by the watchdog, as rc_run.py's reset): read it again
        uint8_t *work = pvPortMalloc(UPDATE_SLOTS_LOAD_BYTES);
        if (work == NULL) {
            return false;
        }
        int loaded = rom_load_partition_table(work, UPDATE_SLOTS_LOAD_BYTES, false);
        vPortFree(work);
        if (loaded != BOOTROM_OK) {
            printf("Update: the partition table could not be read (%d)\n", loaded);
            return false;
        }
        words = rom_get_partition_table_info(info, 3, asked);
    }
    // The first word is the flags the ROM took, without the partition's number
    if (words != 3 || info[0] != (PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION)) {
        printf("Update: partition %u's location not read (%d words, 0x%lx)\n", partition, words,
               (unsigned long) info[0]);
        return false;
    }
    uint32_t first = (info[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
    uint32_t last = (info[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
    *offset = first * FLASH_SECTOR_SIZE;
    *size = (last + 1 - first) * FLASH_SECTOR_SIZE;
    return true;
}


bool update_slots_read(update_slots_t *slots) {
    slots->running = -1;
    slots->on_trial = false;
    slots->free_offset = 0;
    slots->free_size = 0;

    boot_info_t info;
    if (!rom_get_boot_info(&info)) {
        return false;
    }
    slots->on_trial = (info.tbyb_and_update_info & BOOT_TBYB_AND_UPDATE_FLAG_BUY_PENDING) != 0;
    int b = rom_get_b_partition(0);
    if (info.partition == 0 && b > 0) {
        slots->running = 0;
        return update_slots_location((uint8_t) b, &slots->free_offset, &slots->free_size);
    }
    if (info.partition >= 0 && info.partition == b) {
        slots->running = 1;
        return update_slots_location(0, &slots->free_offset, &slots->free_size);
    }
    return false;
}


int update_slots_try_other(void) {
    update_slots_t slots;
    if (!update_slots_read(&slots)) {
        return BOOTROM_ERROR_NOT_FOUND;
    }
    // A flash update boot: the ROM boots the slot that starts at this address first, once, as a trial
    return rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE | REBOOT2_FLAG_NO_RETURN_ON_SUCCESS,
                      UPDATE_SLOTS_TRY_DELAY_MS, XIP_BASE + slots.free_offset, 0);
}


int update_slots_confirm(void) {
    uint8_t *work = pvPortMalloc(UPDATE_SLOTS_BUY_BYTES);
    if (work == NULL) {
        return PICO_ERROR_INSUFFICIENT_RESOURCES;
    }
    // The ROM rewrites the sector that holds the image's mark, with the other core held off flash
    int result = rom_explicit_buy(work, UPDATE_SLOTS_BUY_BYTES);
    vPortFree(work);
    return result;
}

#endif  // PICO_RP2350
