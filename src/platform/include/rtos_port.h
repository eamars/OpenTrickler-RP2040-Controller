#ifndef RTOS_PORT_H_
#define RTOS_PORT_H_

/*
 * What FreeRTOS's ports do differently (ADR-006, gate 7): a task's start, its stack in bytes, and a critical section.
 * The RP chips' port counts a stack in words of 4 bytes and enters a critical section with no argument; ESP-IDF's
 * counts bytes and takes a lock. A platform interface; the RP chips' is platform/rp2/rtos_port_rp2.c. Everything else
 * of FreeRTOS is the same on both and is called as it is.
 */
#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start a task, as xTaskCreate does
 * @param stack_bytes its stack, in bytes: a whole number of words. `stats tasks` gives what each task never touched
 * @param handle where its handle goes, or NULL
 * @return false, and no task, when there was no room for it
 */
bool rtos_task_start(TaskFunction_t run, const char *name, uint32_t stack_bytes, void *arg, UBaseType_t priority,
                     TaskHandle_t *handle);

/*
 * A section no other task, and nothing on the other core, runs in meanwhile: FreeRTOS's critical section, one lock for
 * every section of ours. From a task. It nests; keep it short, for interrupts wait while it is held
 */
void rtos_critical_enter(void);
void rtos_critical_exit(void);

#ifdef __cplusplus
}
#endif

#endif  // RTOS_PORT_H_
