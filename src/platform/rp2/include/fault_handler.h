#ifndef FAULT_HANDLER_H_
#define FAULT_HANDLER_H_

/*
    What the firmware does when it cannot go on (ADR-012, step 2): a hard fault stops one core, and the other may run
    on, so what must not go on by itself is stopped first, by register writes alone. Who has such a thing hands its
    stop over at its start (the motors: the step pulses off, then the drivers); the handler names none of them
    (ADR-006, gate 6).
*/

#ifdef __cplusplus
extern "C" {
#endif

// The stop a hard fault makes first. It runs in the fault: register writes only, no lock, no wait, no RTOS call
void fault_handler_set_stop(void (*stop)(void));

#ifdef __cplusplus
}
#endif

#endif  // FAULT_HANDLER_H_
