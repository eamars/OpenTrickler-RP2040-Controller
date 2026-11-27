#ifndef UPDATE_SLOTS_H_
#define UPDATE_SLOTS_H_

/*
 * The image slots: the part of an update that differs by chip (ADR-004), a platform interface (ADR-006, gate 7).
 * Which slot runs, whether this start is a trial, where the other slot is; what an image says of itself, read as it
 * comes and whole; its write into the free slot, a piece at a time; a restart into a trial of it, the watchdog that
 * ends a trial that hangs, and the confirmation. The RP2350's are its ROM's (update_slots_rp2350.c); the RP2040's are
 * our bootloader's; what both share is update_slots_rp2.c.
 */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int8_t running;             // the slot this image runs from: 0 is A, 1 is B; -1 when it is not known
    bool on_trial;              // this start is a trial: the image confirms itself, or the unit goes back
    uint32_t free_offset;       // the other slot, in flash: its offset from the start and its size
    uint32_t free_size;
    uintptr_t free_read_at;     // where the other slot is read as it lies (the RP2350's window skips its mapping)
    uint32_t free_runs_at;      // the address an image in the other slot must be linked for
    bool linked_per_slot;       // an image runs only from the slot it was linked for (the RP2040's): a release has one
                                // a slot
} update_slots_t;

// An image is written a piece at a time, each this long: a flash sector. Its last piece is filled out with 0xFF
#define UPDATE_SLOTS_PIECE_BYTES 4096

// What an image says of itself
typedef struct {
    bool valid;                 // an image this chip's boot reads: an executable's definition found
    bool this_chip;             // built for this chip, not the other board's
    bool trial;                 // marked to be tried before it is kept: an update image
    uint32_t version;           // its block version, which orders: major in the high half, minor in the low
    bool fits;                  // linked for the free slot
    char board[32];             // the board it was built for; "" when it names none. Read from the whole image only
} update_slots_image_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the slots as this start found them
 * @return false when the chip cannot tell (no partition table, a boot not from a slot); `slots` then says what it can
 */
bool update_slots_read(update_slots_t *slots);

// The block version of the image running, as the boot compares two
uint32_t update_slots_running_version(void);

/**
 * @brief What an image's first piece says, before the rest is in: whether it is one, for this chip, an update, its
 *        version, and whether it fits the free slot (the RP2040's vector table follows its boot2 there), so a wrong
 *        one is refused as it begins. `board` is left ""
 * @param first the image's first bytes, word-aligned
 * @param bytes how many: a piece
 */
void update_slots_read_first(const uint8_t *first, uint32_t bytes, update_slots_image_t *image);

/**
 * @brief The image in the free slot, whole: what it says of itself, its board among it
 * @return false when there is no whole image there: `image` then says nothing
 */
bool update_slots_read_free(const update_slots_t *slots, update_slots_image_t *image);

// Whether the free slot's image holds against its own hash: the whole image read, so a second or more
bool update_slots_free_hash_holds(const update_slots_t *slots);

/**
 * @brief A piece into the free slot, `at` bytes from its start, with the other core held off flash. Nothing outside
 *        the free slot is ever written
 * @return false when the flash was not written, or the piece would pass the slot's end
 */
bool update_slots_write(const update_slots_t *slots, uint32_t at, const uint8_t *piece);

/**
 * @brief Restart into a trial of the image in the other slot. Returns only when the restart could not be asked
 * @return the chip's error
 */
int update_slots_try_other(void);

/**
 * @brief Confirm the image on trial, so it boots from now on. From a task, never an interrupt
 * @return 0, or the chip's error
 */
int update_slots_confirm(void);

/*
 * The trial's watchdog: on at a trial's start, fed every second while the image waits for its network, off once it is
 * confirmed. A trial that hangs, or is no longer fed, is ended by a restart, and the other slot's image boots. Its
 * time is the chip's: on the RP2040 8 s, near its longest
 */
void update_slots_trial_watchdog(bool on);
void update_slots_trial_feed(void);

/*
 * A note for the next start: two words that a watchdog restart keeps and a power cut clears, so the image that runs
 * after a trial can say whether the one tried was kept. The RP2040's watchdog scratch registers 2 and 3 keep them; the
 * RP2350's ROM clears those at a restart (seen 2026-11-16), so it keeps them in its power manager's scratch 2 and 3
 */
void update_slots_note_write(uint32_t first, uint32_t second);
void update_slots_note_read(uint32_t *first, uint32_t *second);

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_SLOTS_H_
