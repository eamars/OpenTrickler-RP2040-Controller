#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "pico.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"

#include "lwip/apps/httpd.h"
#include "lwip/pbuf.h"

#include "firmware_update.h"
#include "common.h"
#include "system_control.h"
#include "charge_mode.h"
#include "eeprom.h"
#include "version.h"
#include "display.h"

extern eeprom_metadata_t metadata;

#define FIRMWARE_UPDATE_UPLOAD_URI "/rest/firmware_upload"
#define FIRMWARE_UPDATE_STATUS_URI "/rest/firmware_update_status"

// The last flash sector of the staging region is reserved entirely for the
// footer (see src/firmware_footer.h) -- payload writes never touch it, so an
// upload's payload and its footer can never land in the same sector.
#define FIRMWARE_UPDATE_MAX_PAYLOAD_SIZE (FIRMWARE_UPDATE_STAGING_SIZE - FLASH_SECTOR_SIZE)
#define FIRMWARE_UPDATE_FOOTER_SECTOR_OFFSET (FIRMWARE_UPDATE_STAGING_OFFSET + FIRMWARE_UPDATE_STAGING_SIZE - FLASH_SECTOR_SIZE)
#define FIRMWARE_UPDATE_FOOTER_OFFSET (FIRMWARE_UPDATE_STAGING_OFFSET + FIRMWARE_UPDATE_STAGING_SIZE - FIRMWARE_FOOTER_SIZE)

typedef struct {
    uint32_t flash_offset;   // offset from the start of flash (XIP_BASE)
    const uint8_t *data;     // exactly `count` bytes
    size_t count;            // multiple of FLASH_SECTOR_SIZE
} flash_write_ctx_t;

// Upload/staging state. Single-instance: only one upload can be in flight at
// a time, matching the reality of a locally-administered device.
static bool s_upload_in_progress;
static bool s_last_upload_ok;
static char s_last_upload_error[64];
static uint32_t s_content_len;
static uint32_t s_payload_size;
static uint32_t s_bytes_received;
static uint32_t s_running_crc32;
static uint32_t s_next_write_offset;   // bytes, relative to FIRMWARE_UPDATE_STAGING_OFFSET
static bool s_write_failed;
static uint8_t s_batch_buf[FIRMWARE_UPDATE_BATCH_SIZE] __attribute__((aligned(4)));
static size_t s_batch_buf_fill;
static uint8_t s_footer_buf[FIRMWARE_FOOTER_SIZE];


// Runs with IRQs disabled and the other FreeRTOS core parked (via
// flash_safe_execute); must not fetch instructions from flash for the
// duration, so this function and everything it calls must live in RAM.
//
// Erases/programs in FLASH_SECTOR_SIZE (4096-byte) steps rather than one
// call covering the whole (much larger) batch: community reports indicate
// flash_range_erase can misbehave for erase sizes that don't divide cleanly
// against the ROM erase routine's internal 64KB block granularity, whereas
// plain sector-sized calls are well-proven. This keeps the call count (and
// thus flash_safe_execute/lockout overhead) exactly as reduced as the
// RAM-side batching already achieves -- only the size of each individual
// hardware erase/program call changes.
static void __no_inline_not_in_flash_func(flash_write_batch)(void *param) {
    flash_write_ctx_t *ctx = (flash_write_ctx_t *)param;

    for (size_t done = 0; done < ctx->count; done += FLASH_SECTOR_SIZE) {
        flash_range_erase(ctx->flash_offset + done, FLASH_SECTOR_SIZE);
        flash_range_program(ctx->flash_offset + done, ctx->data + done, FLASH_SECTOR_SIZE);
    }
}


// Erases and reprograms a batch of consecutive flash sectors in a single
// flash_safe_execute() call, parking the other FreeRTOS core for the whole
// batch's duration. Both flash_range_erase/program natively support a count
// spanning multiple sectors, so no per-sector loop is needed here -- keeping
// the number of flash_safe_execute calls low matters because each call
// dynamically creates and tears down a max-priority FreeRTOS task on the
// other core (see pico_flash's flash.c) to coordinate the lockout; doing
// that once per 4KB sector for a large image (hundreds of times back to
// back) was found to wedge the system. data must point to exactly `count`
// bytes, and count must be a multiple of FLASH_SECTOR_SIZE.
static bool flash_write_batch_safe(uint32_t flash_offset, const uint8_t *data, size_t count) {
    flash_write_ctx_t ctx = {
        .flash_offset = flash_offset,
        .data = data,
        .count = count,
    };

    int rc = flash_safe_execute(flash_write_batch, &ctx, 5000);
    if (rc != PICO_OK) {
        printf("firmware_update: flash_safe_execute failed at offset 0x%08lX count %lu, rc=%d\n",
               (unsigned long)flash_offset, (unsigned long)count, rc);
        return false;
    }

    return true;
}


// Flushes the first `count` bytes of the batch buffer to the staging region
// and advances past them. count must be a multiple of FLASH_SECTOR_SIZE.
static bool flush_batch_buffer(size_t count) {
    uint32_t flash_offset = FIRMWARE_UPDATE_STAGING_OFFSET + s_next_write_offset;
    bool is_ok = flash_write_batch_safe(flash_offset, s_batch_buf, count);

    s_next_write_offset += count;
    s_batch_buf_fill = 0;

    return is_ok;
}


// Draws live OTA progress to the physical LCD, following the same layout
// convention as wireless.c's status screen (title/rule/status lines). This
// is a diagnostic aid: since no serial console is available, whatever is
// last drawn here remains visible on screen if the device ever hangs,
// giving a snapshot of exactly how far the process got. Runs from the
// network/httpd context, concurrently with menu_task's own redraws on the
// other core -- both sides take display_buffer_access_mutex (see
// src/display.c) around their draw sequence so they can't interleave on the
// shared u8g2 buffer and SPI bus.
static void draw_ota_status(const char *phase, uint32_t bytes_done, uint32_t bytes_total, const char *error) {
    u8g2_t *display_handler = get_display_handler();
    char progress_line[24];

    snprintf(progress_line, sizeof(progress_line), "%lu / %lu B", (unsigned long)bytes_done, (unsigned long)bytes_total);

    acquire_display_buffer_access();

    u8g2_ClearBuffer(display_handler);

    u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
    u8g2_DrawStr(display_handler, 5, 10, "Firmware Update");

    u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

    u8g2_SetFont(display_handler, u8g2_font_6x12_tf);
    u8g2_DrawStr(display_handler, 5, 23, phase);
    u8g2_DrawStr(display_handler, 5, 33, progress_line);

    if (error != NULL && error[0] != '\0') {
        u8g2_DrawStr(display_handler, 5, 43, error);
    }

    u8g2_SendBuffer(display_handler);

    release_display_buffer_access();
}


static uint32_t compute_footer_crc32(const firmware_update_footer_t *footer) {
    uint32_t crc = software_crc32_init();
    crc = software_crc32_update(crc, footer, FIRMWARE_FOOTER_SIZE - sizeof(uint32_t));
    return software_crc32_finalize(crc);
}


void firmware_update_init(void) {
    s_upload_in_progress = false;
    s_last_upload_ok = false;
    s_last_upload_error[0] = '\0';
}


bool firmware_update_get_staged_info(firmware_update_footer_t *out) {
    const firmware_update_footer_t *footer =
        (const firmware_update_footer_t *)(XIP_BASE + FIRMWARE_UPDATE_FOOTER_OFFSET);

    if (footer->magic != FIRMWARE_FOOTER_MAGIC) {
        return false;
    }

    if (compute_footer_crc32(footer) != footer->footer_crc32) {
        return false;
    }

    memcpy(out, footer, sizeof(firmware_update_footer_t));
    return true;
}


// Install no longer copies anything into the app region live -- two earlier
// attempts at making that self-overwrite safe under FreeRTOS (per-batch
// flash_safe_execute, then one atomic whole-image flash_safe_execute) each
// bricked the device, the second for reasons that resisted diagnosis without
// serial debugging. The actual copy now happens in the second-stage
// bootloader (bootloader/main.c), which runs before FreeRTOS, WiFi, or the
// second core ever start -- eliminating the multicore/self-overwrite hazard
// entirely rather than trying to make it safe under a live RTOS.
//
// This function's only job is to flip the footer's install-pending flag
// (see FIRMWARE_FOOTER_INSTALL_PENDING_BYTE in firmware_footer.h) and
// reboot. It never touches the app region -- only the staging region's
// already-proven-safe footer sector, via the same flash_write_batch_safe()
// the upload path already uses to commit the footer. A power loss between
// this write and the bootloader completing its copy is safe: the bootloader
// retries the copy from scratch on the next boot until it fully succeeds,
// only then clearing the flag.
bool firmware_update_install_and_reboot(void) {
    firmware_update_footer_t footer;
    if (!firmware_update_get_staged_info(&footer)) {
        return false;
    }

    footer.reserved[FIRMWARE_FOOTER_INSTALL_PENDING_BYTE] = 1;
    footer.footer_crc32 = compute_footer_crc32(&footer);

    memset(s_batch_buf, 0xFF, FLASH_SECTOR_SIZE);
    memcpy(&s_batch_buf[FLASH_SECTOR_SIZE - FIRMWARE_FOOTER_SIZE], &footer, FIRMWARE_FOOTER_SIZE);

    if (!flash_write_batch_safe(FIRMWARE_UPDATE_FOOTER_SECTOR_OFFSET, s_batch_buf, FLASH_SECTOR_SIZE)) {
        draw_ota_status("Install FAILED", 0, footer.payload_size, "Could not mark install pending");
        return false;
    }

    draw_ota_status("Rebooting...", footer.payload_size, footer.payload_size, NULL);

    // Does not return on success -- mirrors the existing s5/software_reset
    // precedent in system_control.c, which also reboots before delivering a
    // response.
    software_reboot();
    return true;
}


err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                        u16_t http_request_len, int content_len, char *response_uri,
                        u16_t response_uri_len, u8_t *post_auto_wnd) {
    (void)http_request;
    (void)http_request_len;

    if (strcmp(uri, FIRMWARE_UPDATE_UPLOAD_URI) != 0) {
        return ERR_ARG;
    }

    if (s_upload_in_progress) {
        s_last_upload_ok = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Upload already in progress");
        snprintf(response_uri, response_uri_len, "%s", FIRMWARE_UPDATE_STATUS_URI);
        return ERR_ARG;
    }

    if (content_len <= (int)FIRMWARE_FOOTER_SIZE ||
        ((uint32_t)content_len - FIRMWARE_FOOTER_SIZE) > FIRMWARE_UPDATE_MAX_PAYLOAD_SIZE) {
        s_last_upload_ok = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Image too large or too small for staging region");
        snprintf(response_uri, response_uri_len, "%s", FIRMWARE_UPDATE_STATUS_URI);
        return ERR_ARG;
    }

    s_content_len = (uint32_t)content_len;
    s_payload_size = s_content_len - FIRMWARE_FOOTER_SIZE;
    s_bytes_received = 0;
    s_running_crc32 = software_crc32_init();
    s_next_write_offset = 0;
    s_batch_buf_fill = 0;
    s_write_failed = false;
    s_upload_in_progress = true;

    // Manual window: we ack (httpd_post_data_recved) every sector's worth of
    // data as it's copied out of the pbuf, independent of the (larger)
    // flash-write batch boundary -- see the ack-cadence comment in
    // httpd_post_receive_data for why this must not be gated on a full
    // batch completing.
    *post_auto_wnd = 0;

    draw_ota_status("Uploading...", 0, s_payload_size, NULL);

    return ERR_OK;
}


// httpd_post_data_recved()'s recved_len is a u16_t (max 65535), but
// FIRMWARE_UPDATE_BATCH_SIZE can exceed that (e.g. 128KB on pico2_w) --
// acking the full amount in one call would silently truncate/overflow to a
// wrong value. Ack in <=65535-byte chunks instead.
static void post_data_recved_all(void *connection, uint32_t total) {
    while (total > 0) {
        u16_t chunk = (total > 0xFFFFu) ? 0xFFFFu : (u16_t)total;
        httpd_post_data_recved(connection, chunk);
        total -= chunk;
    }
}


err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        uint32_t q_start = s_bytes_received;
        uint32_t q_end = q_start + q->len;
        const uint8_t *q_data = (const uint8_t *)q->payload;

        if (q_start < s_payload_size) {
            uint32_t payload_end_in_q = (q_end < s_payload_size) ? q_end : s_payload_size;
            uint32_t remaining = payload_end_in_q - q_start;
            const uint8_t *src = q_data;

            while (remaining > 0) {
                // Ack cadence is intentionally decoupled from flash-write
                // batch size: acking only happens at FIRMWARE_UPDATE_BATCH_SIZE
                // boundaries (32-128KB) deadlocks, because this project's TCP
                // receive window (TCP_WND, ~11.4KB) is smaller than one batch
                // -- the window closes before a batch ever completes, and
                // nothing would ever ack it open again. So each copy is
                // capped at one flash sector, and acked immediately once
                // copied; only the (larger) flash write itself is batched.
                size_t space_to_sector_boundary = FLASH_SECTOR_SIZE - (s_batch_buf_fill % FLASH_SECTOR_SIZE);
                size_t space_in_batch = FIRMWARE_UPDATE_BATCH_SIZE - s_batch_buf_fill;
                size_t chunk = remaining;
                if (chunk > space_to_sector_boundary) {
                    chunk = space_to_sector_boundary;
                }
                if (chunk > space_in_batch) {
                    chunk = space_in_batch;
                }

                memcpy(&s_batch_buf[s_batch_buf_fill], src, chunk);
                s_running_crc32 = software_crc32_update(s_running_crc32, src, chunk);
                s_batch_buf_fill += chunk;
                src += chunk;
                remaining -= chunk;

                // Ack the bytes we just copied out of the pbuf right away --
                // this only signals "consumed from the network", not "safely
                // on flash", so it's correct regardless of whether a flash
                // write happens below.
                post_data_recved_all(connection, chunk);

                // Diagnostic checkpoint: if the device hangs again, whatever
                // was last drawn here shows exactly how far the upload got.
                draw_ota_status("Uploading...", s_next_write_offset + s_batch_buf_fill, s_payload_size, NULL);

                if (s_batch_buf_fill == FIRMWARE_UPDATE_BATCH_SIZE) {
                    // Once a write has failed, the staged image is already
                    // invalid -- stop attempting further flash writes (each
                    // one is expensive/risky), but keep draining incoming
                    // bytes so the connection completes normally and
                    // httpd_post_finished can report a clean error.
                    if (!s_write_failed) {
                        if (!flush_batch_buffer(FIRMWARE_UPDATE_BATCH_SIZE)) {
                            s_write_failed = true;
                        }
                    } else {
                        s_batch_buf_fill = 0;
                    }
                }
            }
        }

        if (q_end > s_payload_size) {
            uint32_t footer_start_in_q = (q_start > s_payload_size) ? q_start : s_payload_size;
            uint32_t footer_bytes = q_end - footer_start_in_q;
            const uint8_t *src = q_data + (footer_start_in_q - q_start);
            uint32_t footer_offset = footer_start_in_q - s_payload_size;

            memcpy(&s_footer_buf[footer_offset], src, footer_bytes);
            post_data_recved_all(connection, footer_bytes);
        }

        s_bytes_received = q_end;
    }

    pbuf_free(p);
    return ERR_OK;
}


void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    (void)connection;

    if (!s_write_failed && s_batch_buf_fill > 0) {
        size_t padded = ((s_batch_buf_fill + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
        memset(&s_batch_buf[s_batch_buf_fill], 0xFF, padded - s_batch_buf_fill);
        if (!flush_batch_buffer(padded)) {
            s_write_failed = true;
        }
    }

    firmware_update_footer_t received_footer;
    memcpy(&received_footer, s_footer_buf, sizeof(received_footer));

    bool is_valid = true;
    s_last_upload_error[0] = '\0';

    if (s_write_failed) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Flash write failed during upload");
    } else if (s_bytes_received != s_content_len) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Incomplete upload");
    } else if (received_footer.magic != FIRMWARE_FOOTER_MAGIC) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Bad footer magic");
    } else if (compute_footer_crc32(&received_footer) != received_footer.footer_crc32) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Footer CRC mismatch");
    } else if (received_footer.board_type != FIRMWARE_UPDATE_BOARD_TYPE) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Wrong board type");
    } else if (received_footer.payload_size != s_payload_size) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Payload size mismatch");
    } else if (software_crc32_finalize(s_running_crc32) != received_footer.payload_crc32) {
        is_valid = false;
        snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Payload CRC mismatch");
    }

    if (is_valid) {
        // Commit the footer to its fixed flash slot (the tail of the
        // reserved last sector) so firmware_update_get_staged_info() can
        // find it, including after a reboot.
        memset(s_batch_buf, 0xFF, FLASH_SECTOR_SIZE);
        memcpy(&s_batch_buf[FLASH_SECTOR_SIZE - FIRMWARE_FOOTER_SIZE], s_footer_buf, FIRMWARE_FOOTER_SIZE);
        is_valid = flash_write_batch_safe(FIRMWARE_UPDATE_FOOTER_SECTOR_OFFSET, s_batch_buf, FLASH_SECTOR_SIZE);
        if (!is_valid) {
            snprintf(s_last_upload_error, sizeof(s_last_upload_error), "Failed to commit footer to flash");
        }
    }

    s_last_upload_ok = is_valid;
    s_upload_in_progress = false;

    draw_ota_status(is_valid ? "Upload OK" : "Upload FAILED",
                     s_bytes_received, s_content_len,
                     is_valid ? NULL : s_last_upload_error);

    snprintf(response_uri, response_uri_len, "%s", FIRMWARE_UPDATE_STATUS_URI);
}


bool http_rest_firmware_update_status(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char json_buffer[640];

    firmware_update_footer_t staged;
    bool staged_valid = firmware_update_get_staged_info(&staged);

    char staged_version[FIRMWARE_FOOTER_VERSION_STRLEN + 1] = {0};
    char staged_hash[FIRMWARE_FOOTER_HASH_STRLEN + 1] = {0};
    uint32_t staged_payload_size = 0;

    if (staged_valid) {
        memcpy(staged_version, staged.version_string, FIRMWARE_FOOTER_VERSION_STRLEN);
        memcpy(staged_hash, staged.vcs_hash, FIRMWARE_FOOTER_HASH_STRLEN);
        staged_payload_size = staged.payload_size;
    }

    // upload_in_progress/bytes_received/content_len/bytes_written reflect
    // either the currently in-flight upload, or the last completed one --
    // useful for polling from a separate browser tab to see live progress
    // (or exactly how far a stalled upload got) without any serial console.
    snprintf(json_buffer, sizeof(json_buffer),
             "%s"
             "{\"s0\":\"%s\",\"s1\":\"%s\",\"s2\":\"%s\",\"s3\":\"%s\","
             "\"staged_valid\":%s,\"staged_version_string\":\"%s\",\"staged_vcs_hash\":\"%s\","
             "\"staged_payload_size\":%lu,\"last_upload_ok\":%s,\"last_upload_error\":\"%s\","
             "\"upload_in_progress\":%s,\"bytes_received\":%lu,\"content_len\":%lu,\"bytes_written\":%lu}",
             http_json_header,
             metadata.unique_id, version_string, vcs_hash, build_type,
             boolean_to_string(staged_valid), staged_version, staged_hash,
             (unsigned long)staged_payload_size,
             boolean_to_string(s_last_upload_ok), s_last_upload_error,
             boolean_to_string(s_upload_in_progress),
             (unsigned long)s_bytes_received, (unsigned long)s_content_len,
             (unsigned long)s_next_write_offset);

    size_t data_length = strlen(json_buffer);
    file->data = json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_firmware_install(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // i0 (bool): confirm install
    static char json_buffer[128];

    bool confirm_flag = false;
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "i0") == 0) {
            confirm_flag = string_to_boolean(values[idx]);
        }
    }

    const char *error_reason = NULL;
    firmware_update_footer_t staged;

    if (!confirm_flag) {
        error_reason = "Install not confirmed";
    } else if (!charge_mode_is_idle()) {
        error_reason = "Charge cycle is active";
    } else if (!firmware_update_get_staged_info(&staged)) {
        error_reason = "No valid staged image";
    }

    if (error_reason != NULL) {
        snprintf(json_buffer, sizeof(json_buffer), "%s{\"error\":\"%s\"}", http_json_header, error_reason);

        size_t data_length = strlen(json_buffer);
        file->data = json_buffer;
        file->len = data_length;
        file->index = data_length;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

        return true;
    }

    // Does not return on success -- marks the image pending and reboots;
    // the bootloader does the actual flash copy on the next boot (see
    // firmware_update_install_and_reboot()'s own comment), mirroring the
    // existing s5/software_reset precedent (no clean response is delivered
    // on the success path).
    firmware_update_install_and_reboot();

    // Only reached if the install failed unexpectedly (e.g. a flash write
    // error marking the pending flag) after passing the checks above.
    snprintf(json_buffer, sizeof(json_buffer), "%s{\"error\":\"Install failed\"}", http_json_header);

    size_t data_length = strlen(json_buffer);
    file->data = json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
