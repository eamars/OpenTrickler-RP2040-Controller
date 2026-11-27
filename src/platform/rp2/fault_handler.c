/*
 * What the firmware does when it cannot go on (ADR-012, step 2).
 *
 * A hard fault stops one core. The other may run on, and with it the task that feeds the step pulses: so the motors
 * are stopped here, first, by register writes alone. Then the core stops where the SDK's own handler stops it, at a
 * breakpoint, for a debug probe to find.
 *
 * A failed assert needs nothing here: the SDK's panic restarts the chip into its USB loader, and a restart stops the
 * PIO.
 */
#include <stdbool.h>
#include <stddef.h>

#include "fault_handler.h"

// The stop handed over (fault_handler.h); NULL until then, when nothing of ours turns yet
static void (*fault_stop)(void) = NULL;


void fault_handler_set_stop(void (*stop)(void)) {
    fault_stop = stop;
}


// The SDK's start-up code declares this weak, as a breakpoint
void isr_hardfault(void) {
    if (fault_stop != NULL) {
        fault_stop();
    }

    __asm volatile ("bkpt #0");
    while (true) {
    }
}
