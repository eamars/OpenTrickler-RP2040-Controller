#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/resets.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"

#if PICO_RP2040
#include "hardware/structs/m0plus.h"
#endif

#if PICO_RP2350
#include "pico/bootrom.h"
#endif

#include "firmware_footer.h"

// This bootloader is deliberately minimal: no FreeRTOS, no networking, no
// display, single core, single-threaded. Its only job, on every boot, is to
// check whether a validated OTA image is staged and pending install and, if
// so, copy it into the app region before chain-loading into the app -- see
// src/firmware_update.c's history for why doing this copy live, under
// FreeRTOS with a second core running, proved unsafe. With nothing else
// running here, the copy needs none of flash_safe_execute's multicore
// lockout machinery: there is no other core or interrupt context that could
// ever fetch a partially-overwritten instruction from the app region while
// this runs.

#define FIRMWARE_UPDATE_FOOTER_OFFSET \
    (FIRMWARE_UPDATE_STAGING_OFFSET + FIRMWARE_UPDATE_STAGING_SIZE - FIRMWARE_FOOTER_SIZE)
#define FIRMWARE_UPDATE_FOOTER_SECTOR_OFFSET \
    (FIRMWARE_UPDATE_STAGING_OFFSET + FIRMWARE_UPDATE_STAGING_SIZE - FLASH_SECTOR_SIZE)

static uint8_t s_sector_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

// Standalone CRC32 (same standard reflected algorithm, same init/xorout, as
// src/common.c's software_crc32_* -- reimplemented here rather than shared,
// since common.c pulls in FreeRTOS and EEPROM dependencies this target must
// not link).
static uint32_t compute_crc32(const void *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;

    while (length--) {
        crc ^= *p++;
        for (int bit = 0; bit < 8; bit += 1) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint32_t compute_footer_crc32(const firmware_update_footer_t *footer) {
    return compute_crc32(footer, FIRMWARE_FOOTER_SIZE - sizeof(uint32_t));
}

static bool read_and_validate_footer(firmware_update_footer_t *out) {
    const firmware_update_footer_t *footer =
        (const firmware_update_footer_t *)(XIP_BASE + FIRMWARE_UPDATE_FOOTER_OFFSET);

    if (footer->magic != FIRMWARE_FOOTER_MAGIC) {
        return false;
    }
    if (footer->board_type != FIRMWARE_UPDATE_BOARD_TYPE) {
        return false;
    }
    if (compute_footer_crc32(footer) != footer->footer_crc32) {
        return false;
    }

    memcpy(out, footer, sizeof(*out));
    return true;
}

// Invalidates the staged image entirely (erasing the footer sector removes
// its magic, so firmware_update_get_staged_info() correctly reports "no
// image staged" afterward) -- not just the install-pending flag. Without
// this, the web UI kept showing the just-installed version as still
// "staged", since a valid-but-pending-cleared footer is indistinguishable
// from a valid-and-ready-to-install one from the REST status's point of
// view. Only ever called after copy_staged_image_to_app_region() has fully
// completed, so a power loss before this point simply leaves the original
// footer (install_pending still set) in place and the copy retries from
// scratch (from the untouched staging-region source data) on the next boot
// -- see the plan's "power-loss safety" design decision.
static void invalidate_staged_image(void) {
    flash_range_erase(FIRMWARE_UPDATE_FOOTER_SECTOR_OFFSET, FLASH_SECTOR_SIZE);
}

// Plain flash_range_erase/program calls, no flash_safe_execute: this is the
// only code running on the only core that's started, so there is nothing to
// lock out.
static void copy_staged_image_to_app_region(uint32_t payload_size) {
    uint32_t total_bytes = ((payload_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

    for (uint32_t offset = 0; offset < total_bytes; offset += FLASH_SECTOR_SIZE) {
        const uint8_t *src = (const uint8_t *)(XIP_BASE + FIRMWARE_UPDATE_STAGING_OFFSET + offset);
        memcpy(s_sector_buf, src, FLASH_SECTOR_SIZE);

        flash_range_erase(FIRMWARE_UPDATE_APP_REGION_OFFSET + offset, FLASH_SECTOR_SIZE);
        flash_range_program(FIRMWARE_UPDATE_APP_REGION_OFFSET + offset, s_sector_buf, FLASH_SECTOR_SIZE);
    }
}

// Chain-loads into the app region. Never returns.
static void __attribute__((noreturn)) chain_to_app(void) {
#if PICO_RP2350
    // Official bootrom API for exactly this: validates the app's own
    // IMAGE_DEF within the given window and transfers control to it.
    // pico_add_extra_outputs already embeds IMAGE_DEF in the app build by
    // default, so no changes are needed there.
    rom_chain_image(s_sector_buf, sizeof(s_sector_buf),
                     XIP_BASE + FIRMWARE_UPDATE_APP_REGION_OFFSET, FIRMWARE_UPDATE_APP_REGION_SIZE);
    // Only reached if the app region has no valid image to chain into.
    while (true) {
        tight_loop_contents();
    }
#else
    // RP2040 has no chain-load ROM API. This mirrors exactly what boot2
    // itself does for a normal (directly-booted) app: the app region's
    // first 256 bytes are its own (unused, never executed here) .boot2
    // slot, immediately followed by its vector table -- same layout a
    // directly-booted image already has (confirmed via objdump earlier in
    // this project's history).
    uint32_t app_vector_table = XIP_BASE + FIRMWARE_UPDATE_APP_REGION_OFFSET + 0x100u;
    uint32_t *vtable = (uint32_t *)app_vector_table;
    uint32_t app_sp = vtable[0];
    uint32_t app_reset_handler = vtable[1];

    // Mirror picowota's proven jump sequence exactly, not just PRIMASK: a
    // plain save_and_disable_interrupts() leaves individual NVIC enable/
    // pending bits and every peripheral's own state untouched. Confirmed by
    // testing that skipping this leaves the app's LCD working but its
    // CYW43/PIO-based wireless init broken specifically after the
    // bootloader has just run a real copy (more of the bootloader's own
    // init has executed by then than on a pure pass-through chain-load).
    // reset_block puts every peripheral except QSPI IO/pads, SYSCFG, and
    // PLL_SYS (needed to keep executing from flash) back into power-on-
    // reset state -- including PIO -- so the app's own init starts from the
    // same clean slate it would on a real cold boot. No corresponding
    // unreset_block() call: the app's own runtime_init() un-resets what it
    // needs, same as after any normal boot.
    save_and_disable_interrupts();
    ppb_hw->syst_csr &= ~M0PLUS_SYST_CSR_ENABLE_BITS;
    ppb_hw->nvic_icer = 0xFFFFFFFFu;
    ppb_hw->nvic_icpr = 0xFFFFFFFFu;
    reset_block(~(RESETS_RESET_IO_QSPI_BITS |
                  RESETS_RESET_PADS_QSPI_BITS |
                  RESETS_RESET_SYSCFG_BITS |
                  RESETS_RESET_PLL_SYS_BITS));

    scb_hw->vtor = app_vector_table;
    __asm volatile (
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r" (app_sp), "r" (app_reset_handler)
    );
    while (true) {
        tight_loop_contents();
    }
#endif
}

int main(void) {
    firmware_update_footer_t footer;

    if (read_and_validate_footer(&footer) && footer.reserved[FIRMWARE_FOOTER_INSTALL_PENDING_BYTE] != 0) {
        copy_staged_image_to_app_region(footer.payload_size);
        invalidate_staged_image();
    }

    chain_to_app();
}
