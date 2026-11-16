/*
 * Firmware updates (ADR-004): the part that is the same on every chip. At start-up the slots are read; an image
 * booted on trial arms the watchdog and feeds it while it waits for the network, and confirms itself once the unit
 * is on the network it is set up for and its web server listens. An image that does not confirm itself in time is
 * left: the watchdog restarts the unit, and the other slot's image boots.
 */
#include "update.h"

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "boot/picobin.h"
#include "hardware/watchdog.h"
#include "mbedtls/sha256.h"

#include "activity.h"
#include "picobin_image.h"
#include "update_slots.h"
#include "version.h"

#if PICO_RP2350
#define UPDATE_CHIP PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2350
#else
#define UPDATE_CHIP PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2040
#endif
#define UPDATE_HASH_BYTES 32

#define UPDATE_TRIAL_WATCHDOG_MS 8000       // the watchdog while on trial: the RP2040's longest is 8.3 s
#define UPDATE_TRIAL_FEED_MS 1000
#define UPDATE_TRIAL_LIMIT_MS 60000         // fed no longer than this: a trial that is not on its network by then ends

#ifndef PICO_CRT0_VERSION_MAJOR
#define PICO_CRT0_VERSION_MAJOR 0
#endif
#ifndef PICO_CRT0_VERSION_MINOR
#define PICO_CRT0_VERSION_MINOR 0
#endif

static update_slots_t update_slots;
static bool update_slots_known = false;
static volatile bool update_confirmed = false;
static volatile int update_confirm_result = 0;
static TickType_t update_trial_started = 0;
static TimerHandle_t update_feed_timer = NULL;


static char update_slot_name(int8_t slot) {
    return slot == 0 ? 'a' : (slot == 1 ? 'b' : '-');
}


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
    uint32_t header = image->load_map[0];
    uint32_t entries = (header << 1) >> 25;
    if ((header >> 31) != 0) {
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
        held = stored >= start && stored + bytes <= start + size &&
               mbedtls_sha256_update(&sha, (const uint8_t *) stored, bytes) == 0;
    }
    uint32_t block_words = image->hash_def[1] & 0xffff;
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


/*
 * The image in the free slot, checked before it is tried: an executable for this chip, marked for a trial, newer
 * than the one running, linked for the free slot, its hash holding. NULL when it may be tried, else why not
 */
static const char *update_check_free(void) {
    picobin_image_t image;
    if (!picobin_image_read(update_slots.free_read_at, update_slots.free_size, &image)) {
        return "no image in the free slot";
    }
    if (image.chip != UPDATE_CHIP) {
        return "an image for the other chip";
    }
    if (!image.trial) {
        return "not an update image (no try-before-you-buy mark)";
    }
    uint32_t running = ((uint32_t) PICO_CRT0_VERSION_MAJOR << 16) | PICO_CRT0_VERSION_MINOR;
    if (picobin_image_version(&image) <= running) {
        return "not newer than the image running";
    }
    // The first load map entry is the image's start, where it runs: the slot's address on the RP2040, flash's start
    // on the RP2350, which maps the slot there
    if (image.load_map == NULL || ((image.load_map[0] << 1) >> 25) == 0 ||
        image.load_map[2] != update_slots.free_runs_at) {
        return "not linked for the free slot";
    }
    if (!update_hash_holds(&image, update_slots.free_read_at, update_slots.free_size)) {
        return "its hash does not hold";
    }
    return NULL;
}


// While on trial: the watchdog is fed until the image confirms itself or the trial's time runs out
static void update_feed(TimerHandle_t timer) {
    if (update_confirmed) {
        xTimerStop(timer, 0);
        return;
    }
    if ((xTaskGetTickCount() - update_trial_started) >= pdMS_TO_TICKS(UPDATE_TRIAL_LIMIT_MS)) {
        printf("Update: not confirmed within %d s; the watchdog restarts into the other slot\n",
               UPDATE_TRIAL_LIMIT_MS / 1000);
        xTimerStop(timer, 0);
        return;
    }
    watchdog_update();
}


void update_init(void) {
    update_slots_known = update_slots_read(&update_slots);
    printf("Update: running slot %c, version %s (block %d.%d)%s\n", update_slot_name(update_slots.running),
           version_string, PICO_CRT0_VERSION_MAJOR, PICO_CRT0_VERSION_MINOR,
           update_slots.on_trial ? ", on trial: it confirms itself once the unit is on its network" : "");
    if (!update_slots.on_trial) {
        return;
    }
    watchdog_enable(UPDATE_TRIAL_WATCHDOG_MS, true);
    update_trial_started = xTaskGetTickCount();
    update_feed_timer = xTimerCreate("Update feed", pdMS_TO_TICKS(UPDATE_TRIAL_FEED_MS), pdTRUE, NULL, update_feed);
    if (update_feed_timer == NULL || xTimerStart(update_feed_timer, 0) != pdPASS) {
        printf("Update: no timer to feed the trial's watchdog; the unit goes back\n");
    }
}


void update_network_ready(bool on_its_network) {
    if (!update_slots.on_trial || update_confirmed) {
        return;
    }
    if (!on_its_network) {
        printf("Update: not on its network; not confirmed, the unit goes back at the trial's end\n");
        return;
    }
#if RC_TEST_NO_CONFIRM
    // A test build that never confirms itself (rc_run.py slot_trial --expect back)
    printf("Update: a test build: not confirmed\n");
    return;
#endif
    update_confirm_result = update_slots_confirm();
    if (update_confirm_result != 0) {
        printf("Update: the confirmation failed (%d); the unit goes back\n", update_confirm_result);
        return;
    }
    update_confirmed = true;
    watchdog_disable();
    printf("Update: slot %c confirmed\n", update_slot_name(update_slots.running));
}


bool update_console(const char *args, char *reply, size_t reply_size) {
    while (*args == ' ') {
        args += 1;
    }
    if (*args == '\0') {
        snprintf(reply, reply_size,
                 "@ok update slot=%c known=%d trial=%d confirmed=%d confirm_result=%d ver=%s block=%d.%d "
                 "free=0x%lx+0x%lx",
                 update_slot_name(update_slots.running), update_slots_known, update_slots.on_trial,
                 update_confirmed, update_confirm_result, version_string, PICO_CRT0_VERSION_MAJOR,
                 PICO_CRT0_VERSION_MINOR, (unsigned long) update_slots.free_offset,
                 (unsigned long) update_slots.free_size);
        return true;
    }
    if (strcmp(args, "try") == 0) {
        if (activity_get() != ACTIVITY_NONE) {
            snprintf(reply, reply_size, "@err busy update try: %s runs", activity_name(activity_get()));
            return false;
        }
        if (!update_slots_known) {
            snprintf(reply, reply_size, "@err not_found update try: the slots are not known");
            return false;
        }
        const char *refused = update_check_free();
        if (refused != NULL) {
            snprintf(reply, reply_size, "@err refused update try: %s", refused);
            return false;
        }
        printf("@ok update try: restarting into a trial of slot %c\n", update_slot_name(1 - update_slots.running));
        int result = update_slots_try_other();
        snprintf(reply, reply_size, "@err failed update try: the restart was refused (%d)", result);
        return false;
    }
    snprintf(reply, reply_size, "@err usage update [try]");
    return false;
}
