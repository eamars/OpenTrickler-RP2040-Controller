#ifndef PICOBIN_IMAGE_H_
#define PICOBIN_IMAGE_H_

/*
 * An image's metadata, read from its blocks (ADR-004): the SDK's image format on both chips (boot/picobin.h), the
 * same the RP2350's ROM reads. Used by the RP2040's bootloader and by the firmware, so it depends on nothing: no
 * RTOS, no printf. An image is read where it lies, through any window onto flash (XIP, or the RP2350's untranslated
 * one); the blocks' links and the load map's storage are relative, so they hold in either.
 */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;                         // an executable's image definition was found
    uint8_t chip;                       // PICOBIN_IMAGE_TYPE_EXE_CHIP_RP2040 or _RP2350
    bool trial;                         // marked try-before-you-buy: not yet confirmed
    uint16_t major;                     // the version: 0.0 when the image carries none
    uint16_t minor;
    const uint32_t *block;              // the last image definition's block
    const uint32_t *image_type;         // its image type item, which holds the mark
    const uint32_t *load_map;           // its load map item, or NULL
    const uint32_t *hash_def;           // its hash definition item, or NULL
    const uint8_t *hash_value;          // its SHA-256, 32 bytes, or NULL
} picobin_image_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the image in a region: its first block within the region's first 4 KB, then the block loop
 * @param start the region's start, as read (an XIP address)
 * @param size the region's size
 * @return image->valid
 */
bool picobin_image_read(uintptr_t start, uint32_t size, picobin_image_t *image);

/**
 * @brief The version as one number that orders: major in the high half, minor in the low
 */
static inline uint32_t picobin_image_version(const picobin_image_t *image) {
    return ((uint32_t) image->major << 16) | image->minor;
}

#ifdef __cplusplus
}
#endif

#endif  // PICOBIN_IMAGE_H_
