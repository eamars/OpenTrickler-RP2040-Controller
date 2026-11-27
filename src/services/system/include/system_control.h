#ifndef SYSTEM_CONTROL_H_
#define SYSTEM_CONTROL_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


// Restart the unit, after what was handed to system_before_restart(). Does not return
int software_reboot(void);

/*
 * Something to end before the unit restarts (ADR-006, gate 6): the web joins with its clients, each told the unit
 * has gone and not left with a request that hangs. The restart names no web. Handed over at start-up; run in the
 * order handed over, in the task that restarts. False when there is no room for it.
 */
bool system_before_restart(void (*end)(void));

// Run them: for a restart the system does not make itself (the update's, into a trial of the other slot)
void system_prepare_restart(void);

// The board this image is built for, as a release's files name it: "pico_w", "pico2_w"
const char *system_board_name(void);


#ifdef __cplusplus
}
#endif


#endif  // 