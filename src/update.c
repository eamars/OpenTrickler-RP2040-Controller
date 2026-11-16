/*
 * Firmware updates (ADR-004): the part that is the same on every chip.
 *
 * At start-up the slots are read. An image booted on trial arms the watchdog and feeds it while it waits for the
 * network, and confirms itself once the unit is on the network it is set up for and its web server listens; one that
 * does not confirm itself in time is left: the watchdog restarts the unit, and the other slot's image boots.
 *
 * An update is a job (api_v1.md, "Firmware updates"): the API asks it, the menu takes it and update_mode_menu() holds
 * the screen while the image comes in pieces through the web server's task and is written into the free slot, a
 * sector at a time. Its first block is checked in its first 4 KB, the whole once it is in. Install restarts into a
 * trial of it. Before that restart a note is left where a watchdog restart keeps it (update_slots_note_write), so
 * the image that runs after it can say whether the one tried was kept (confirmed) or left (back).
 */
#include "update.h"

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "mbedtls/sha256.h"
#include "pico/binary_info/defs.h"
#include "pico/binary_info/structure.h"
#include "pico/flash.h"
#include "u8g2.h"

#include "activity.h"
#include "api_v1.h"
#include "display.h"
#include "events.h"
#include "menu.h"
#include "mini_12864_module.h"
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
#define UPDATE_RECEIVING_IDLE_MS 30000      // a job receiving that gets no bytes for this long ends (p)
#define UPDATE_READY_LIMIT_MS 60000         // a job ready and not installed within this ends (p)
#define UPDATE_MODE_TICK_MS 250
#define UPDATE_INSTALL_DELAY_MS 300         // the install's reply gets out first

// The note left for the start after a trial (update_slots_note_write): the slot tried, and its version
#define UPDATE_NOTE_MAGIC 0x55504400u       // "UPD", the slot in the low byte

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

// The job, under the activity lock
static volatile update_state_t update_state = UPDATE_IDLE;
static update_last_t update_last = {.result = UPDATE_RESULT_NONE};
static uint32_t update_job_version = 0;     // the block version of the image the job takes: 0 until its first block is in
static uint8_t *update_sector = NULL;       // a sector of the image as it comes: from the heap at the first job, kept
static uint32_t update_sector_fill = 0;
static uint32_t update_written = 0;         // the bytes written into the free slot, whole sectors
static uint32_t update_taken = 0;           // the image's bytes taken so far: `received`
static uint32_t update_expected = 0;        // the image's length, as its PUT gives it
/*
 * The upload the job takes, by its token (review Q1): one at a time. A second is refused, and a piece, end or abort
 * of any other (a second connection, or one left from a job that ended) changes nothing. 0 when none is open
 */
static uint32_t update_upload_token = 0;
static uint32_t update_upload_tokens = 0;
static TickType_t update_tick = 0;          // the job's last bytes, or when it became ready
static update_refusal_t update_upload_refusal = {0};
static volatile bool update_giving_back = false;   // the mode has returned; the menu is not yet back at its forms


// --- Versions and the slots' names

static char update_slot_name(int8_t slot) {
    return slot == 0 ? 'a' : (slot == 1 ? 'b' : '-');
}


static uint32_t update_running_version(void) {
    return ((uint32_t) PICO_CRT0_VERSION_MAJOR << 16) | PICO_CRT0_VERSION_MINOR;
}


// A block version as the firmware's version string has it: 276.900 is 1.20.900 (scripts/gen_version.py)
void update_version_text(uint32_t version, char *out, size_t size) {
    uint32_t major = version >> 16;
    snprintf(out, size, "%lu.%lu.%lu", (unsigned long) (major >> 8), (unsigned long) (major & 0xffu),
             (unsigned long) (version & 0xffffu));
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


static update_refusal_t update_refuse(const char *code, const char *message) {
    return (update_refusal_t) {.code = code, .message = message};
}


// What the first block says, which the first 4 KB hold: an executable for this chip, an update, newer
static update_refusal_t update_check_first(const picobin_image_t *image) {
    if (!image->valid) {
        return update_refuse("not_an_image", "no image definition in its first 4 KB");
    }
    if (image->chip != UPDATE_CHIP) {
        return update_refuse("wrong_chip", "an image for the other chip: the other board's");
    }
    if (!image->trial) {
        return update_refuse("not_an_update", "not an update image: it carries no try-before-you-buy mark");
    }
    if (picobin_image_version(image) <= update_running_version()) {
        return update_refuse("not_newer", "not newer than the image running");
    }
    return update_refuse(NULL, NULL);
}


/*
 * The image in the free slot, whole: as its first block, then linked for the free slot, built for this board (its
 * binary information) and its hash holding. On the
 * RP2040 the first load map entry is the image's start, where it runs: the slot's address; on the RP2350 flash's
 * start, which the ROM maps the slot to
 */
static update_refusal_t update_check_free(picobin_image_t *image) {
    if (!picobin_image_read(update_slots.free_read_at, update_slots.free_size, image)) {
        return update_refuse("not_an_image", "no whole image in the free slot");
    }
    update_refusal_t first = update_check_first(image);
    if (first.code != NULL) {
        return first;
    }
    if (image->load_map == NULL || ((image->load_map[0] << 1) >> 25) == 0 ||
        image->load_map[2] != update_slots.free_runs_at) {
        return update_refuse("wrong_slot", "not linked for the free slot");
    }
    char board[32];
    if (!update_image_board(update_slots.free_read_at, update_slots.free_size, update_slots.free_runs_at, board,
                            sizeof(board)) || strcmp(board, api_v1_board_name()) != 0) {
        return update_refuse("wrong_board", "built for another board");
    }
    if (!update_hash_holds(image, update_slots.free_read_at, update_slots.free_size)) {
        return update_refuse("hash_mismatch", "its hash does not hold");
    }
    return update_refuse(NULL, NULL);
}


// --- A trial, and the note it leaves for the next start

static void update_clear_note(void) {
    update_slots_note_write(0, 0);
}


// Restart into a trial of the free slot's image, leaving the note. Returns only when the restart was refused, and then
// the note goes too: no trial ran (review C3)
static int update_try(uint32_t version) {
    update_slots_note_write(UPDATE_NOTE_MAGIC | (uint32_t) (1 - update_slots.running), version);
    int refused = update_slots_try_other();
    update_clear_note();
    return refused;
}


// An update's event: a refusal's code, or the block version of the image (kept as a number, so a kept event never
// changes; review C1)
static void update_event(event_kind_t kind, activity_by_t by, event_reason_t reason, const char *code,
                         uint32_t version) {
    event_t event = {.source = EVENT_SOURCE_UPDATE, .kind = (uint8_t) kind, .by = (uint8_t) by,
                     .reason = (uint8_t) reason, .text = code, .version = version};
    events_add(&event);
}


// At start-up: the note says a trial was asked before this start. The trial running keeps it until it confirms
// itself; any other image running means the one tried was left
static void update_read_note(void) {
    uint32_t note, version;
    update_slots_note_read(&note, &version);
    if ((note & ~0xffu) != UPDATE_NOTE_MAGIC) {
        return;
    }
    int8_t tried = (int8_t) (note & 0xffu);
    if (tried == update_slots.running && update_slots.on_trial) {
        return;
    }
    update_clear_note();
    update_last = (update_last_t) {.result = UPDATE_RESULT_BACK, .version = version};
    update_event(EVENT_BACK, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, NULL, version);
    char text[16];
    update_version_text(version, text, sizeof(text));
    printf("Update: the image tried (%s) was left: back on slot %c\n", text, update_slot_name(update_slots.running));
}


// --- Start-up and the trial

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
    update_read_note();
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
    update_clear_note();
    activity_lock();
    update_last = (update_last_t) {.result = UPDATE_RESULT_CONFIRMED, .version = update_running_version()};
    activity_unlock();
    update_event(EVENT_CONFIRMED, ACTIVITY_BY_UNIT, EVENT_REASON_NONE, NULL, update_running_version());
    printf("Update: slot %c confirmed\n", update_slot_name(update_slots.running));
}


// --- The job

void update_status(update_status_t *status) {
    activity_lock();
    status->state = update_state;
    activity_t asked;
    if (status->state == UPDATE_IDLE && activity_start_pending(&asked) && asked == ACTIVITY_UPDATE) {
        // Asked, and the menu has not begun it: read from the request itself, so a start dropped or stopped before it
        // began is idle again at once (review C3)
        status->state = UPDATE_STARTING;
    }
    else if (status->state == UPDATE_IDLE && update_giving_back) {
        // Idle once the menu is back at its forms, where a start is taken again
        if (menu_get_current_form_id() >= 0) {
            update_giving_back = false;
        }
        else {
            status->state = UPDATE_ENDING;
        }
    }
    status->slot = update_slot_name(update_slots.running);
    status->on_trial = update_slots.on_trial && !update_confirmed;
    status->free_bytes = update_slots_known ? update_slots.free_size : 0;
    status->received = status->state == UPDATE_IDLE || status->state == UPDATE_STARTING ? 0 : update_taken;
    status->last = update_last;
    activity_unlock();
    status->version = version_string;
    update_version_text(update_running_version(), status->block, sizeof(status->block));
    // The release manifest's name for the image this unit takes: one for the Pico 2 W, one a slot for the Pico W
#if PICO_RP2350
    snprintf(status->takes, sizeof(status->takes), "%s", api_v1_board_name());
#else
    snprintf(status->takes, sizeof(status->takes), "%s_%c", api_v1_board_name(),
             update_slot_name(1 - update_slots.running));
#endif
}


update_start_result_t update_request_start(activity_by_t by) {
    if (!update_slots_known) {
        return UPDATE_START_NO_SLOTS;
    }
    if (update_slots.on_trial && !update_confirmed) {
        // The free slot holds the image the unit goes back to until this one is confirmed: it is not written (review D2)
        return UPDATE_START_ON_TRIAL;
    }
    activity_t asked_before;
    if (activity_get() != ACTIVITY_NONE || activity_start_pending(&asked_before) || menu_get_current_form_id() < 0) {
        return UPDATE_START_BUSY;
    }
    activity_lock();
    bool asked = update_state == UPDATE_IDLE && activity_request_start(ACTIVITY_UPDATE, by);
    activity_unlock();
    if (!asked) {
        return UPDATE_START_BUSY;
    }
    menu_wake();
    return UPDATE_START_OK;
}


// The job ends: under the activity lock, from either task. The mode sees the state and gives the screen back
static void update_end_locked(update_result_t result, activity_by_t by, update_refusal_t refusal) {
    update_state = UPDATE_ENDING;
    update_upload_token = 0;    // an upload still open is the ended job's: nothing of it is taken now
    update_last = (update_last_t) {.result = result, .code = refusal.code, .message = refusal.message,
                                   .version = update_job_version};
    event_reason_t reason = result == UPDATE_RESULT_REFUSED ? EVENT_REASON_REFUSED :
                            (result == UPDATE_RESULT_TIME ? EVENT_REASON_TIME : EVENT_REASON_STOPPED);
    update_event(EVENT_STOPPED, by, reason, refusal.code, 0);
    menu_wake();    // the mode gives the screen back now, not at its next tick
}


bool update_install(void) {
    activity_lock();
    bool ready = update_state == UPDATE_READY;
    if (ready) {
        update_state = UPDATE_INSTALLING;
        update_event(EVENT_INSTALLING, ACTIVITY_BY_API, EVENT_REASON_NONE, NULL, update_job_version);
    }
    activity_unlock();
    return ready;
}


// --- The image, in pieces (the web server's task)

static void update_flash_sector(void *param) {
    uint32_t offset = *(const uint32_t *) param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, update_sector, FLASH_SECTOR_SIZE);
}


// The sector in the buffer, into the free slot. The first is checked before it is written. Nothing outside the free
// slot is ever written: the bound is held here, where flash is written (review Q1)
static bool update_write_sector(void) {
    if (update_written + FLASH_SECTOR_SIZE > update_slots.free_size) {
        update_upload_refusal = update_refuse("too_large", "larger than the free slot");
        return false;
    }
    if (update_written == 0) {
        picobin_image_t first;
        picobin_image_read_first((uintptr_t) update_sector, FLASH_SECTOR_SIZE, &first);
        update_upload_refusal = update_check_first(&first);
        if (update_upload_refusal.code == NULL && !update_slots_first_fits(update_sector, FLASH_SECTOR_SIZE)) {
            update_upload_refusal = update_refuse("wrong_slot", "not linked for the free slot");
        }
        if (update_upload_refusal.code != NULL) {
            return false;
        }
        update_job_version = picobin_image_version(&first);
    }
    uint32_t offset = update_slots.free_offset + update_written;
    // Flash is written with the other core held off it; no job but this one runs, so no motor waits for a feed
    if (flash_safe_execute(update_flash_sector, &offset, UINT32_MAX) != PICO_OK) {
        update_upload_refusal = update_refuse("failed", "the flash could not be written");
        return false;
    }
    update_written += FLASH_SECTOR_SIZE;
    update_sector_fill = 0;
    return true;
}


update_refusal_t update_image_begin(uint32_t bytes, int *status, uint32_t *token) {
    *token = 0;
    // The size first: a body refused changes nothing of the job (review Q1)
    if (bytes > update_slots.free_size) {
        *status = 413;
        return update_refuse("too_large", "larger than the free slot");
    }
    if (bytes == 0) {
        *status = 400;
        return update_refuse("not_an_image", "an empty body");
    }
    activity_lock();
    bool receiving = update_state == UPDATE_RECEIVING;
    bool taken = receiving && update_upload_token == 0;
    if (taken) {
        update_upload_tokens += 1;
        if (update_upload_tokens == 0) {
            update_upload_tokens = 1;
        }
        update_upload_token = update_upload_tokens;
        *token = update_upload_token;
        update_written = 0;
        update_sector_fill = 0;
        update_taken = 0;
        update_expected = bytes;
        update_tick = xTaskGetTickCount();
        update_upload_refusal = update_refuse(NULL, NULL);
    }
    activity_unlock();
    if (!receiving) {
        *status = 409;
        return update_refuse("not_running", "no update is receiving: POST /api/v1/update/start first");
    }
    if (!taken) {
        *status = 409;
        return update_refuse("busy", "another upload of the image is under way: one at a time");
    }
    *status = 200;
    return update_refuse(NULL, NULL);
}


bool update_image_piece(uint32_t token, const uint8_t *data, size_t len) {
    while (len > 0) {
        if (update_state != UPDATE_RECEIVING || token != update_upload_token ||
            update_taken + len > update_expected) {
            if (update_upload_refusal.code == NULL) {
                update_upload_refusal = update_refuse("not_running", "the update was stopped");
            }
            return false;
        }
        size_t take = FLASH_SECTOR_SIZE - update_sector_fill;
        if (take > len) {
            take = len;
        }
        memcpy(update_sector + update_sector_fill, data, take);
        update_sector_fill += (uint32_t) take;
        update_taken += (uint32_t) take;
        data += take;
        len -= take;
        if (update_sector_fill == FLASH_SECTOR_SIZE && !update_write_sector()) {
            return false;
        }
    }
    update_tick = xTaskGetTickCount();
    return true;
}


update_refusal_t update_image_end(uint32_t token, int *status) {
    if (token != update_upload_token) {
        // An upload the job no longer takes: its job ended while it came
        *status = 409;
        return update_refuse("not_running", "the update was stopped");
    }
    picobin_image_t image;
    update_refusal_t refusal = update_upload_refusal;
    if (refusal.code == NULL && update_sector_fill > 0) {
        // The last sector, filled out as erased flash
        memset(update_sector + update_sector_fill, 0xff, FLASH_SECTOR_SIZE - update_sector_fill);
        if (!update_write_sector()) {
            refusal = update_upload_refusal;
        }
    }
    if (refusal.code == NULL) {
        refusal = update_check_free(&image);
    }
    activity_lock();
    bool ours = token == update_upload_token;
    update_upload_token = ours ? 0 : update_upload_token;
    if (update_state != UPDATE_RECEIVING || !ours) {
        // Stopped, or ended by its time, while the image came
        refusal = update_refuse("not_running", "the update was stopped");
        *status = 409;
    } else if (refusal.code == NULL) {
        update_state = UPDATE_READY;
        update_tick = xTaskGetTickCount();
        update_event(EVENT_RECEIVED, ACTIVITY_BY_API, EVENT_REASON_NONE, NULL, update_job_version);
        *status = 200;
    } else {
        // A refused image ends the job: nothing of it is installed
        *status = strcmp(refusal.code, "failed") == 0 ? 500 : 400;
        update_end_locked(UPDATE_RESULT_REFUSED, ACTIVITY_BY_API, refusal);
    }
    activity_unlock();
    return refusal;
}


void update_image_abort(uint32_t token) {
    // The client went: the job waits for the image again, from its start. Another's going changes nothing
    activity_lock();
    if (token == update_upload_token) {
        update_upload_token = 0;
        update_written = 0;
        update_sector_fill = 0;
        update_taken = 0;
    }
    activity_unlock();
}


// --- The mode: the job holds the screen (the menu's task)

static void update_draw(update_state_t state) {
    u8g2_t *display = get_display_handler();
    char line[32];
    u8g2_ClearBuffer(display);
    u8g2_SetFont(display, u8g2_font_helvB08_tr);
    u8g2_DrawStr(display, 5, 10, "Firmware update");
    u8g2_DrawHLine(display, 0, 13, u8g2_GetDisplayWidth(display));
    u8g2_SetFont(display, u8g2_font_helvR08_tr);
    if (state == UPDATE_RECEIVING && update_expected > 0) {
        snprintf(line, sizeof(line), "Receiving %lu%%",
                 (unsigned long) ((uint64_t) update_taken * 100u / update_expected));
    } else if (state == UPDATE_RECEIVING || state == UPDATE_STARTING) {
        snprintf(line, sizeof(line), "Waiting for the image");
    } else if (state == UPDATE_READY) {
        char version[16];
        update_version_text(update_job_version, version, sizeof(version));
        snprintf(line, sizeof(line), "Ready: %s", version);
    } else {
        snprintf(line, sizeof(line), "Installing, restarting");
    }
    u8g2_DrawStr(display, 5, 25, line);
    u8g2_DrawStr(display, 5, 37, "Keep the power on.");
    u8g2_DrawStr(display, 5, 49, state == UPDATE_INSTALLING ? "" : "Press RST to stop.");
    u8g2_SendBuffer(display);
}


uint8_t update_mode_menu(void) {
    activity_lock();
    activity_job_begin(ACTIVITY_UPDATE);
    activity_by_t by = activity_job_by();
    if (update_sector == NULL) {
        update_sector = pvPortMalloc(FLASH_SECTOR_SIZE);
    }
    if (update_sector == NULL) {
        update_end_locked(UPDATE_RESULT_STOPPED, ACTIVITY_BY_UNIT, update_refuse("failed", "no room for a sector"));
    } else {
        update_state = UPDATE_RECEIVING;
        update_written = 0;
        update_sector_fill = 0;
        update_taken = 0;
        update_expected = 0;
        update_upload_token = 0;    // an upload left open from an earlier job is not this one's
        update_job_version = 0;
        update_tick = xTaskGetTickCount();
        update_event(EVENT_STARTED, by, EVENT_REASON_NONE, NULL, 0);
    }
    activity_unlock();

    while (true) {
        update_state_t state = update_state;
        if (state == UPDATE_ENDING || state == UPDATE_IDLE) {
            break;
        }
        if (state == UPDATE_INSTALLING) {
            update_draw(state);
            vTaskDelay(pdMS_TO_TICKS(UPDATE_INSTALL_DELAY_MS));
            picobin_image_t image;
            picobin_image_read(update_slots.free_read_at, update_slots.free_size, &image);
            int refused = update_try(picobin_image_version(&image));
            // Only when the restart was refused
            activity_lock();
            update_end_locked(UPDATE_RESULT_STOPPED, ACTIVITY_BY_UNIT,
                              update_refuse("failed", "the restart into the trial was refused"));
            activity_unlock();
            printf("Update: the restart into the trial was refused (%d)\n", refused);
            break;
        }
        update_draw(state);
        ButtonEncoderEvent_t button = button_wait_for_input_ms(UPDATE_MODE_TICK_MS);
        activity_by_t stop_by;
        bool stop = activity_take_stop(&stop_by);
        if (button == BUTTON_RST_PRESSED) {
            stop = true;
            stop_by = ACTIVITY_BY_BUTTON;
        }
        activity_lock();
        // Signed: the server's task may set update_tick after the tick count was read (review Q2)
        int32_t since = (int32_t) (xTaskGetTickCount() - update_tick);
        if (update_state == UPDATE_RECEIVING || update_state == UPDATE_READY) {
            if (stop) {
                update_end_locked(UPDATE_RESULT_STOPPED, stop_by, update_refuse(NULL, NULL));
            } else if ((update_state == UPDATE_RECEIVING &&
                        since >= (int32_t) pdMS_TO_TICKS(UPDATE_RECEIVING_IDLE_MS)) ||
                       (update_state == UPDATE_READY && since >= (int32_t) pdMS_TO_TICKS(UPDATE_READY_LIMIT_MS))) {
                update_end_locked(UPDATE_RESULT_TIME, ACTIVITY_BY_UNIT, update_refuse(NULL, NULL));
            }
        }
        activity_unlock();
    }
    activity_job_end();
    activity_lock();
    update_state = UPDATE_IDLE;
    update_giving_back = true;
    activity_unlock();
    return 1;
}


// --- The console

bool update_console(const char *args, char *reply, size_t reply_size) {
    while (*args == ' ') {
        args += 1;
    }
    if (*args == '\0') {
        update_status_t status;
        update_status(&status);
        snprintf(reply, reply_size,
                 "@ok update slot=%c known=%d trial=%d confirmed=%d confirm_result=%d ver=%s block=%d.%d "
                 "free=0x%lx+0x%lx state=%d takes=%s last=%d",
                 update_slot_name(update_slots.running), update_slots_known, update_slots.on_trial,
                 update_confirmed, update_confirm_result, version_string, PICO_CRT0_VERSION_MAJOR,
                 PICO_CRT0_VERSION_MINOR, (unsigned long) update_slots.free_offset,
                 (unsigned long) update_slots.free_size, (int) status.state, status.takes, (int) status.last.result);
        return true;
    }
    if (strcmp(args, "try") == 0) {
        if (update_slots.on_trial && !update_confirmed) {
            snprintf(reply, reply_size, "@err busy update try: the image running is on trial, not yet confirmed");
            return false;
        }
        if (activity_get() != ACTIVITY_NONE) {
            snprintf(reply, reply_size, "@err busy update try: %s runs", activity_name(activity_get()));
            return false;
        }
        if (!update_slots_known) {
            snprintf(reply, reply_size, "@err not_found update try: the slots are not known");
            return false;
        }
        picobin_image_t image;
        update_refusal_t refused = update_check_free(&image);
        if (refused.code != NULL) {
            snprintf(reply, reply_size, "@err refused update try: %s: %s", refused.code, refused.message);
            return false;
        }
        printf("@ok update try: restarting into a trial of slot %c\n", update_slot_name(1 - update_slots.running));
        int result = update_try(picobin_image_version(&image));
        snprintf(reply, reply_size, "@err failed update try: the restart was refused (%d)", result);
        return false;
    }
    snprintf(reply, reply_size, "@err usage update [try]");
    return false;
}
