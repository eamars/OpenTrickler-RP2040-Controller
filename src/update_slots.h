#ifndef UPDATE_SLOTS_H_
#define UPDATE_SLOTS_H_

/*
 * The image slots: the part of an update that differs by chip (ADR-004). Which slot runs, whether this start is a
 * trial, where the other slot is, a restart into a trial of it, and the confirmation. The RP2350's are its ROM's
 * (update_slots_rp2350.c); the RP2040's are our bootloader's.
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
} update_slots_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the slots as this start found them
 * @return false when the chip cannot tell (no partition table, a boot not from a slot); `slots` then says what it can
 */
bool update_slots_read(update_slots_t *slots);

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

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_SLOTS_H_
