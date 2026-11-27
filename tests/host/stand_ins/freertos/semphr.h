// The host's stand-in for FreeRTOS's semphr.h: only the type the headers the host tests include name (tests/host_test.py)
#ifndef HOST_SEMPHR_H_
#define HOST_SEMPHR_H_

#include "FreeRTOS.h"
#include "task.h"          // FreeRTOS's semphr.h brings it, through queue.h

typedef void *SemaphoreHandle_t;

#endif
