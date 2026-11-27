/*
 * The image slots on both RP chips (ADR-004; update_slots.h): an image as the SDK's image format has it (its metadata
 * blocks, picobin_image.c), written into the free slot a sector at a time, and the watchdog that ends a trial. What
 * differs between the RP2040 and the RP2350 is in update_slots_rp2040.c and update_slots_rp2350.c.
 */
#include "update_slots.h"

#include <string.h>

#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "mbedtls/sha256.h"
#include "pico/binary_info/defs.h"
#include "pico/binary_info/structure.h"
#include "pico/flash.h"

#include "picobin_image.h"
#include "update_slots_chip.h"

#if PICO_RP2350
#define UPDATE_CHIP PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2350
#else
#define UPDATE_CHIP PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2040
#endif
#define UPDATE_HASH_BYTES 32

#define UPDATE_TRIAL_WATCHDOG_MS 8000       // the watchdog while on trial: the RP2040's longest is 8.3 s

#ifndef PICO_CRT0_VERSION_MAJOR
#define PICO_CRT0_VERSION_MAJOR 0
#endif
#ifndef PICO_CRT0_VERSION_MINOR
#define PICO_CRT0_VERSION_MINOR 0
#endif

_Static_assert(UPDATE_SLOTS_PIECE_BYTES == FLASH_SECTOR_SIZE, "an image's piece is a flash sector");


uint32_t update_slots_running_version(void) {
    return ((uint32_t) PICO_CRT0_VERSION_MAJOR << 16) | PICO_CRT0_VERSION_MINOR;
}


// --- The check of an image

/*
 * An image's SHA-256, as picotool seals it (bintool.cpp, 2.3.1): the data of each load map entry from where it is
 * stored (a cleared region by its size), then the block's words up to and including the hash definition, with the
 * try-before-you-buy mark cleared in the copy hashed, so the hash holds once the image is confirmed. Checked against
 * an image picotool verified (progress, 2026-11-16). Only relative load maps, stored inside the slot, are taken
 */
static bool update_hash_holds(const picobin_image_t *image, uintptr_t start, uint32_t size) {
    if (image->load_map == NULL || image->hash_def == NULL || image->hash_value == NULL ||
        (image->hash_def[0] >> 24) != PICOBIN_HASH_SHA256) {
        return false;
    }
    // Every length and place below is the image's own: each is held inside its item and the slot (review Q3)
    uint32_t header = image->load_map[0];
    uint32_t entries = (header << 1) >> 25;
    uint32_t item_words = (header >> 8) & 0xffu;
    if ((header >> 31) != 0 || 1 + 3 * entries > item_words) {
        return false;
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    bool held = mbedtls_sha256_starts(&sha, 0) == 0;
    for (uint32_t idx = 0; held && idx < entries; idx += 1) {
        int32_t storage_rel = (int32_t) image->load_map[1 + 3 * idx];
        uint32_t bytes = image->load_map[3 + 3 * idx];
        if (storage_rel == 0) {
            held = mbedtls_sha256_update(&sha, (const uint8_t *) &bytes, sizeof(bytes)) == 0;
            continue;
        }
        uintptr_t stored = (uintptr_t) image->load_map + storage_rel;
        held = stored >= start && bytes <= size && stored - start <= size - bytes &&
               mbedtls_sha256_update(&sha, (const uint8_t *) stored, bytes) == 0;
    }
    uint32_t block_words = image->hash_def[1] & 0xffff;
    held = held && (uintptr_t) image->block >= start && block_words <= (start + size - (uintptr_t) image->block) / 4;
    for (uint32_t idx = 0; held && idx < block_words; idx += 1) {
        uint32_t word = image->block[idx];
        if (idx == 1 && (word & 0xff) == PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE) {
            word &= ~((uint32_t) PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS << 16);
        }
        held = mbedtls_sha256_update(&sha, (const uint8_t *) &word, sizeof(word)) == 0;
    }
    uint8_t digest[UPDATE_HASH_BYTES];
    held = held && mbedtls_sha256_finish(&sha, digest) == 0 && memcmp(digest, image->hash_value, sizeof(digest)) == 0;
    mbedtls_sha256_free(&sha);
    return held;
}


#define UPDATE_BINARY_INFO_SEARCH_BYTES 1024     // the binary information's header lies near the image's start


/*
 * The board an image was built for: its binary information's pico_board, which picotool shows. The image is read
 * where it lies, and its pointers are where it runs, so each is moved from runs_at into the window it is read
 * through. False when the image names none that lies inside it
 */
static bool update_image_board(uintptr_t read_at, uint32_t size, uint32_t runs_at, char *out, size_t out_size) {
    const uint32_t *words = (const uint32_t *) read_at;
    for (uint32_t idx = 0; (idx + 5) * 4 <= UPDATE_BINARY_INFO_SEARCH_BYTES; idx += 1) {
        if (words[idx] != BINARY_INFO_MARKER_START || words[idx + 4] != BINARY_INFO_MARKER_END) {
            continue;
        }
        // The image's own pointers: each on a word and inside the slot, or not read (review Q3)
        uint32_t start = words[idx + 1];
        uint32_t end = words[idx + 2];
        if (start < runs_at || end < start || end - runs_at > size || ((start | end) & 3u) != 0) {
            return false;
        }
        for (uint32_t at = start; at < end; at += 4) {
            uint32_t entry = *(const uint32_t *) (read_at + (at - runs_at));
            if (entry < runs_at || entry - runs_at > size - 12 || (entry & 3u) != 0) {
                continue;   // one kept in RAM: not the board's
            }
            const uint32_t *item = (const uint32_t *) (read_at + (entry - runs_at));
            if ((item[0] & 0xffffu) != BINARY_INFO_TYPE_ID_AND_STRING || (item[0] >> 16) != BINARY_INFO_TAG_RASPBERRY_PI ||
                item[1] != BINARY_INFO_ID_RP_PICO_BOARD) {
                continue;
            }
            uint32_t text = item[2];
            size_t len = 0;
            while (len + 1 < out_size && text >= runs_at && text - runs_at < size - len &&
                   ((const char *) (read_at + (text - runs_at)))[len] != '\0') {
                out[len] = ((const char *) (read_at + (text - runs_at)))[len];
                len += 1;
            }
            out[len] = '\0';
            return len > 0;
        }
        return false;
    }
    return false;
}


// What the image's first block says, into what the interface gives
static void update_slots_image_from(const picobin_image_t *picobin, update_slots_image_t *image) {
    image->valid = picobin->valid;
    image->this_chip = picobin->chip == UPDATE_CHIP;
    image->trial = picobin->trial;
    image->version = picobin_image_version(picobin);
}


void update_slots_read_first(const uint8_t *first, uint32_t bytes, update_slots_image_t *image) {
    memset(image, 0, sizeof(*image));
    picobin_image_t picobin;
    picobin_image_read_first((uintptr_t) first, bytes, &picobin);
    update_slots_image_from(&picobin, image);
    image->fits = update_slots_first_fits(first, bytes);
}


/*
 * The image in the free slot, whole. On the RP2040 the first load map entry is the image's start, where it runs: the
 * slot's address; on the RP2350 flash's start, which the ROM maps the slot to
 */
bool update_slots_read_free(const update_slots_t *slots, update_slots_image_t *image) {
    memset(image, 0, sizeof(*image));
    picobin_image_t picobin;
    if (!picobin_image_read(slots->free_read_at, slots->free_size, &picobin)) {
        return false;
    }
    update_slots_image_from(&picobin, image);
    image->fits = picobin.load_map != NULL && ((picobin.load_map[0] << 1) >> 25) != 0 &&
                  picobin.load_map[2] == slots->free_runs_at;
    if (!update_image_board(slots->free_read_at, slots->free_size, slots->free_runs_at, image->board,
                            sizeof(image->board))) {
        image->board[0] = '\0';
    }
    return true;
}


bool update_slots_free_hash_holds(const update_slots_t *slots) {
    picobin_image_t picobin;
    return picobin_image_read(slots->free_read_at, slots->free_size, &picobin) &&
           update_hash_holds(&picobin, slots->free_read_at, slots->free_size);
}


// --- The image, written

typedef struct {
    uint32_t offset;
    const uint8_t *piece;
} update_slots_write_t;


static void update_flash_sector(void *param) {
    const update_slots_write_t *write = param;
    flash_range_erase(write->offset, FLASH_SECTOR_SIZE);
    flash_range_program(write->offset, write->piece, FLASH_SECTOR_SIZE);
}


// Nothing outside the free slot is ever written: the bound is held here, where flash is written (review Q1)
bool update_slots_write(const update_slots_t *slots, uint32_t at, const uint8_t *piece) {
    if (at > slots->free_size || slots->free_size - at < UPDATE_SLOTS_PIECE_BYTES) {
        return false;
    }
    update_slots_write_t write = {.offset = slots->free_offset + at, .piece = piece};
    // Flash is written with the other core held off it
    return flash_safe_execute(update_flash_sector, &write, UINT32_MAX) == PICO_OK;
}


// --- The trial's watchdog

void update_slots_trial_watchdog(bool on) {
    if (on) {
        watchdog_enable(UPDATE_TRIAL_WATCHDOG_MS, true);
    }
    else {
        watchdog_disable();
    }
}


void update_slots_trial_feed(void) {
    watchdog_update();
}
