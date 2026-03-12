#ifndef FLASH_STORAGE_H_
#define FLASH_STORAGE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// Flash Storage Configuration
// Pico W has 2MB flash. Reserve last 8KB for persistent storage:
// - 0x1FE000 (2MB - 8KB): ML history sector (4KB)
// - 0x1FF000 (2MB - 4KB): Reserved for future use
//
// Note: FLASH_SECTOR_SIZE is 4KB, FLASH_PAGE_SIZE is 256 bytes
// flash_range_erase() erases in sector units, flash_range_program() writes in page units

#define FLASH_STORAGE_ML_HISTORY_OFFSET     (2 * 1024 * 1024 - 8 * 1024)  // 0x1FE000
#define FLASH_STORAGE_ML_HISTORY_SIZE       (4 * 1024)                     // 4KB sector

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize flash storage subsystem
 * Must be called after FreeRTOS scheduler starts
 */
void flash_storage_init(void);

/**
 * Read ML history from flash
 * @param data Buffer to read into
 * @param len Number of bytes to read (must not exceed sector size)
 * @return true on success
 */
bool flash_ml_history_read(uint8_t* data, size_t len);

/**
 * Write ML history to flash
 * Erases the sector and writes new data
 * @param data Data to write
 * @param len Number of bytes to write (must not exceed sector size)
 * @return true on success
 */
bool flash_ml_history_write(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // FLASH_STORAGE_H_
