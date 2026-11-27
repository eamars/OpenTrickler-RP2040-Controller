#ifndef CHIP_H_
#define CHIP_H_

/*
 * The system (ADR-006, gate 7): what the firmware asks of the chip itself. The time since the start, a busy wait, a
 * restart, the chip's own id, a random number and the console. A platform interface; the RP chips' is the Pico SDK's
 * (platform/rp2/chip_rp2.c). What a hard fault stops first is fault_handler.h.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Microseconds since the chip started; it does not wrap in the unit's life. From a task or an interrupt
uint64_t chip_time_us(void);

// A wait with the core spinning, no RTOS: before the scheduler starts, or for microseconds between two writes
void chip_busy_wait_us(uint32_t us);

/*
 * A restart of the whole chip, `after_ms` from now (0: at once), as the watchdog makes one: what a watchdog restart
 * keeps (update_slots.h's note) is kept. It returns at once; the restart follows, whatever the caller does meanwhile
 */
void chip_restart(uint32_t after_ms);

// The chip's own id: the same at every start, different on every chip. The RP chips': the flash's unique id
#define CHIP_ID_BYTES 8
void chip_id(uint8_t id[CHIP_ID_BYTES]);

// A random number from the chip's own source, for what must differ from one start to the next (the API's boot)
uint32_t chip_random32(void);

/*
 * The console (ADR-001): the RP chips' is stdio over RTT, which the debug probe reads from the chip's RAM. Started
 * first of all, so every printf after it reaches it; a printf is its output
 */
void chip_console_start(void);

// A character typed on the console, waited for at most `timeout_us` (0: only if one is there); CHIP_CONSOLE_NONE if none
#define CHIP_CONSOLE_NONE (-1)
int chip_console_read(uint32_t timeout_us);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_H_
