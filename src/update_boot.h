#ifndef UPDATE_BOOT_H_
#define UPDATE_BOOT_H_

/*
 * The RP2040's boot path (ADR-004), shared by its bootloader (bootloader/bootloader.c) and the firmware
 * (update_slots_rp2040.c). The slots' places come from targets/flash_layout.cmake, given to both as definitions.
 *
 * The rules are the RP2350 ROM's: of the two slots, the good image with the higher version boots; an image marked
 * try-before-you-buy boots only when the running image asks for a trial, once; the bootloader arms the watchdog
 * before it jumps into an image on trial, and an image that does not confirm itself in time is left at the restart.
 * A trial is asked through the watchdog's scratch registers 0 and 1, which a watchdog restart keeps and a power cut
 * clears (the SDK's own reboots use 4 to 7). The bootloader clears them as it reads them: a trial is one start.
 */
#include <stdbool.h>
#include <stdint.h>

#include "hardware/regs/addressmap.h"

#ifndef UPDATE_SLOT_A_OFFSET
#error "UPDATE_SLOT_A_OFFSET, UPDATE_SLOT_B_OFFSET and UPDATE_SLOT_SIZE come from targets/flash_layout.cmake"
#endif

#define UPDATE_BOOT_SCRATCH_TRY 0
#define UPDATE_BOOT_SCRATCH_CHECK 1             // the complement of the request, so a stray value asks nothing
#define UPDATE_BOOT_SCRATCH_NOTE 2              // 2 and 3: the firmware's note for the next start (update_slots.h)
#define UPDATE_BOOT_TRY_MAGIC 0x54525900u       // "TRY", and the slot in the low byte
#define UPDATE_BOOT_TRIAL_WATCHDOG_MS 8000      // the RP2040's watchdog reaches 8.3 s at most
#define UPDATE_BOOT_VECTORS_AT 0x100            // a slot's image starts with its boot2, then its vector table

static inline uint32_t update_boot_slot_offset(int slot) {
    return slot == 0 ? UPDATE_SLOT_A_OFFSET : UPDATE_SLOT_B_OFFSET;
}

/*
 * Whether an image's vector table is one linked for the slot that starts at slot_start (an XIP address): a stack in
 * RAM, a reset handler inside the slot. The bootloader boots no image that fails it, and an update refuses one in its
 * first 4 KB, which hold it (`wrong_slot`), rather than once the whole image is in
 */
static inline bool update_boot_vectors_fit(const uint32_t *vectors, uintptr_t slot_start) {
    return vectors[0] > SRAM_BASE && vectors[0] <= SRAM_END && vectors[1] > slot_start &&
           vectors[1] < slot_start + UPDATE_SLOT_SIZE;
}

#endif  // UPDATE_BOOT_H_
