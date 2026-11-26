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

#include "motors.h"


// The SDK's start-up code declares this weak, as a breakpoint
void isr_hardfault(void) {
    motors_fault_stop();

    __asm volatile ("bkpt #0");
    while (true) {
    }
}
