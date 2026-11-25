// The host's stand-in for FreeRTOS's task.h (tests/host_test.py)
#ifndef HOST_TASK_H_
#define HOST_TASK_H_

#include "FreeRTOS.h"

#define taskSCHEDULER_RUNNING 2

static inline void vTaskDelay(TickType_t ticks) {
    (void) ticks;
}

#endif
