/*
 * The RP2040's bootloader (ADR-004): at the start of flash, before slot A and slot B (src/update_boot.h). It reads
 * both slots' metadata blocks, picks the image to boot by the RP2350 ROM's rules, and jumps into it. It never writes
 * flash, and with no image to boot it starts the USB loader, so a unit with nothing to run shows up as a drive
 * without BOOTSEL. It is changed only by USB, so it does only this. No RTOS, no stdio, one core.
 *
 * The jump is PR #111's (its author's, 2026), proven on a Pico W: every peripheral but the flash's and the system
 * clock's put back in reset first, or the radio's start fails after the bootloader has run.
 */
#include <stdbool.h>
#include <stdint.h>

#include "pico.h"
#include "boot/picobin.h"
#include "hardware/regs/addressmap.h"
#include "hardware/resets.h"
#include "hardware/structs/m0plus.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

#include "picobin_image.h"
#include "update_boot.h"


// An image that may boot from this slot: an executable for the RP2040, linked for this slot
static bool bootloader_image(int slot, picobin_image_t *image) {
    uintptr_t start = XIP_BASE + update_boot_slot_offset(slot);
    if (!picobin_image_read(start, UPDATE_SLOT_SIZE, image) || image->chip != PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2040) {
        return false;
    }
    if (image->load_map == NULL || ((image->load_map[0] << 1) >> 25) == 0 || image->load_map[2] != start) {
        return false;
    }
    // Its vector table: a stack in RAM, a reset handler inside the slot
    const uint32_t *vectors = (const uint32_t *) (start + UPDATE_BOOT_VECTORS_AT);
    return vectors[0] > SRAM_BASE && vectors[0] <= SRAM_END && vectors[1] > start &&
           vectors[1] < start + UPDATE_SLOT_SIZE;
}


// The trial the running image asked, or -1; asked for this start only
static int bootloader_trial_asked(void) {
    uint32_t asked = watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_TRY];
    uint32_t check = watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_CHECK];
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_TRY] = 0;
    watchdog_hw->scratch[UPDATE_BOOT_SCRATCH_CHECK] = 0;
    if ((asked & ~0xffu) != UPDATE_BOOT_TRY_MAGIC || check != ~asked || (asked & 0xffu) > 1) {
        return -1;
    }
    return (int) (asked & 0xffu);
}


static void __attribute__((noreturn)) bootloader_jump(int slot) {
    uint32_t vector_table = XIP_BASE + update_boot_slot_offset(slot) + UPDATE_BOOT_VECTORS_AT;
    const uint32_t *vectors = (const uint32_t *) vector_table;
    uint32_t stack = vectors[0];
    uint32_t reset_handler = vectors[1];

    save_and_disable_interrupts();
    ppb_hw->syst_csr &= ~M0PLUS_SYST_CSR_ENABLE_BITS;
    ppb_hw->nvic_icer = 0xffffffffu;
    ppb_hw->nvic_icpr = 0xffffffffu;
    reset_block(~(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS | RESETS_RESET_SYSCFG_BITS |
                  RESETS_RESET_PLL_SYS_BITS));
    scb_hw->vtor = vector_table;
    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(stack), "r"(reset_handler));
    while (true) {
        tight_loop_contents();
    }
}


int main(void) {
    picobin_image_t images[2];
    bool bootable[2];
    for (int slot = 0; slot < 2; slot += 1) {
        bootable[slot] = bootloader_image(slot, &images[slot]);
    }

    int trial = bootloader_trial_asked();
    if (trial >= 0 && bootable[trial] && images[trial].trial) {
        watchdog_enable(UPDATE_BOOT_TRIAL_WATCHDOG_MS, true);
        bootloader_jump(trial);
    }

    // Otherwise the confirmed image with the higher version; slot A on a tie
    int boot = -1;
    for (int slot = 0; slot < 2; slot += 1) {
        if (bootable[slot] && !images[slot].trial &&
            (boot < 0 || picobin_image_version(&images[slot]) > picobin_image_version(&images[boot]))) {
            boot = slot;
        }
    }
    if (boot < 0) {
        reset_usb_boot(0, 0);
    }
    bootloader_jump(boot);
}
