#include "flash_storage.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>
#include <stdio.h>

// Flash storage is memory-mapped at XIP_BASE + offset
static const uint8_t* ml_history_flash_ptr = (const uint8_t*)(XIP_BASE + FLASH_STORAGE_ML_HISTORY_OFFSET);

// Static buffer for write operations (must persist during flash_safe_execute)
static uint8_t g_write_buffer[FLASH_STORAGE_ML_HISTORY_SIZE] __attribute__((aligned(4)));
static size_t g_write_len = 0;

void flash_storage_init(void) {
    printf("Flash storage: ML history at offset 0x%X (XIP 0x%08X)\n",
           FLASH_STORAGE_ML_HISTORY_OFFSET,
           (unsigned int)(XIP_BASE + FLASH_STORAGE_ML_HISTORY_OFFSET));
}

bool flash_ml_history_read(uint8_t* data, size_t len) {
    if (data == NULL || len == 0 || len > FLASH_STORAGE_ML_HISTORY_SIZE) {
        return false;
    }

    // Direct read from XIP-mapped flash - no special handling needed
    memcpy(data, ml_history_flash_ptr, len);
    return true;
}

// Callback for flash_safe_execute - performs the actual erase + program
static void flash_write_callback(void* param) {
    (void)param;

    // Erase the sector first (4KB minimum)
    flash_range_erase(FLASH_STORAGE_ML_HISTORY_OFFSET, FLASH_SECTOR_SIZE);

    // Program in page-sized chunks (256 bytes)
    size_t bytes_written = 0;
    while (bytes_written < g_write_len) {
        size_t chunk_size = g_write_len - bytes_written;
        if (chunk_size > FLASH_PAGE_SIZE) {
            chunk_size = FLASH_PAGE_SIZE;
        }

        // Pad to page size if needed (flash_range_program requires page-aligned writes)
        uint8_t page_buffer[FLASH_PAGE_SIZE];
        memset(page_buffer, 0xFF, FLASH_PAGE_SIZE);  // 0xFF is erased state
        memcpy(page_buffer, g_write_buffer + bytes_written, chunk_size);

        flash_range_program(FLASH_STORAGE_ML_HISTORY_OFFSET + bytes_written,
                           page_buffer, FLASH_PAGE_SIZE);
        bytes_written += FLASH_PAGE_SIZE;
    }
}

bool flash_ml_history_write(const uint8_t* data, size_t len) {
    if (data == NULL || len == 0 || len > FLASH_STORAGE_ML_HISTORY_SIZE) {
        printf("Flash storage: Invalid write parameters (len=%u)\n", (unsigned int)len);
        return false;
    }

    // Copy data to static buffer (must persist during flash_safe_execute)
    memcpy(g_write_buffer, data, len);
    g_write_len = len;

    // Use flash_safe_execute for FreeRTOS-safe flash programming
    // This handles multi-core coordination and disables interrupts during flash ops
    int rc = flash_safe_execute(flash_write_callback, NULL, 1000);  // 1 second timeout

    if (rc != PICO_OK) {
        printf("Flash storage: flash_safe_execute failed with %d\n", rc);
        return false;
    }

    // Verify write
    if (memcmp(ml_history_flash_ptr, data, len) != 0) {
        printf("Flash storage: Write verification failed\n");
        return false;
    }

    return true;
}
