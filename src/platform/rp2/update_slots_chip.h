#ifndef UPDATE_SLOTS_CHIP_H_
#define UPDATE_SLOTS_CHIP_H_

/*
 * What the RP2040's and the RP2350's slot files give the part both chips share (update_slots_rp2.c). Not an
 * interface: the platform's own.
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Whether an image's first 4 KB say it is linked for the free slot, so a wrong one is refused as it begins
 *        (`wrong_slot`). The RP2040's vector table follows its boot2 there; the RP2350's images run from either slot
 * @param first the image's first bytes, word-aligned
 * @param bytes how many: a sector
 */
bool update_slots_first_fits(const uint8_t *first, uint32_t bytes);

#endif  // UPDATE_SLOTS_CHIP_H_
