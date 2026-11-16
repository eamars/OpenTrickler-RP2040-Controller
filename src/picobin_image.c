/*
 * An image's metadata, read from its blocks (ADR-004, boot/picobin.h). A block is a start marker, items, a last item
 * that gives the items' size, the relative link to the next block, and an end marker; the blocks form a loop. The
 * last image definition in the loop is the image's (picotool's seal appends one, with its hash).
 */
#include "picobin_image.h"

#include <stddef.h>

#include "boot/picobin.h"

#define PICOBIN_IMAGE_FIRST_BLOCK_BYTES 0x1000  // the first block lies within the image's first 4 KB
#define PICOBIN_IMAGE_MAX_BLOCKS 8              // a loop longer than this is not an image of ours
#define PICOBIN_IMAGE_MAX_ITEMS 32


static bool picobin_image_inside(uintptr_t address, uint32_t bytes, uintptr_t start, uint32_t size) {
    return address >= start && address + bytes <= start + size && address + bytes >= address;
}


// One block's items. Returns the address of the next block, or 0 when the block is not whole
static uintptr_t picobin_image_block(uintptr_t at, uintptr_t start, uint32_t size, picobin_image_t *found) {
    const uint32_t *word = (const uint32_t *) at;
    if (!picobin_image_inside(at, 4, start, size) || word[0] != PICOBIN_BLOCK_MARKER_START) {
        return 0;
    }
    picobin_image_t block = {.block = word};
    bool image_def = false;
    uint32_t index = 1;
    for (uint32_t items = 0; items < PICOBIN_IMAGE_MAX_ITEMS; items += 1) {
        if (!picobin_image_inside((uintptr_t) &word[index], 4, start, size)) {
            return 0;
        }
        uint32_t header = word[index];
        uint8_t type = header & 0xff;
        uint32_t words = (type & 0x80) ? ((header >> 8) & 0xffff) : ((header >> 8) & 0xff);
        if (type == PICOBIN_BLOCK_ITEM_2BS_LAST) {
            // The items' size, then the link and the end marker
            if (!picobin_image_inside((uintptr_t) &word[index + 1], 8, start, size) || words != index - 1 ||
                word[index + 2] != PICOBIN_BLOCK_MARKER_END) {
                return 0;
            }
            if (image_def) {
                *found = block;
            }
            return at + (uintptr_t) (int32_t) word[index + 1];
        }
        if (words == 0 || !picobin_image_inside((uintptr_t) &word[index], words * 4, start, size)) {
            return 0;
        }
        const uint32_t *item = &word[index];
        if (type == PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE) {
            uint16_t flags = header >> 16;
            if ((flags & PICOBIN_IMAGE_TYPE_IMAGE_TYPE_BITS) == PICOBIN_IMAGE_TYPE_IMAGE_TYPE_EXE) {
                image_def = true;
                block.valid = true;
                block.chip = (flags & PICOBIN_IMAGE_TYPE_EXE_CHIP_BITS) >> PICOBIN_IMAGE_TYPE_EXE_CHIP_LSB;
                block.trial = (flags & PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS) != 0;
                block.image_type = item;
            }
        } else if (type == PICOBIN_BLOCK_ITEM_1BS_VERSION && words >= 2) {
            block.minor = item[1] & 0xffff;
            block.major = item[1] >> 16;
        } else if (type == PICOBIN_BLOCK_ITEM_LOAD_MAP) {
            block.load_map = item;
        } else if (type == PICOBIN_BLOCK_ITEM_1BS_HASH_DEF) {
            block.hash_def = item;
        } else if (type == PICOBIN_BLOCK_ITEM_HASH_VALUE && words >= 9) {
            block.hash_value = (const uint8_t *) &item[1];
        }
        index += words;
    }
    return 0;
}


bool picobin_image_read(uintptr_t start, uint32_t size, picobin_image_t *image) {
    *image = (picobin_image_t) {0};
    uintptr_t first = 0;
    for (uintptr_t at = start; at < start + PICOBIN_IMAGE_FIRST_BLOCK_BYTES && at + 4 <= start + size; at += 4) {
        if (*(const uint32_t *) at == PICOBIN_BLOCK_MARKER_START) {
            first = at;
            break;
        }
    }
    if (first == 0) {
        return false;
    }
    uintptr_t at = first;
    for (uint32_t blocks = 0; blocks < PICOBIN_IMAGE_MAX_BLOCKS; blocks += 1) {
        at = picobin_image_block(at, start, size, image);
        if (at == 0) {
            *image = (picobin_image_t) {0};     // a broken loop is no image
            return false;
        }
        if (at == first) {
            return image->valid;
        }
    }
    *image = (picobin_image_t) {0};
    return false;
}
