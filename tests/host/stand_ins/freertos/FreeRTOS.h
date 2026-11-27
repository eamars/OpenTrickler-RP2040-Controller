// The host's stand-in for FreeRTOS's header: only what the files the host tests compile use (tests/host_test.py)
#ifndef HOST_FREERTOS_H_
#define HOST_FREERTOS_H_

#include <stdbool.h>        // the SDK's headers bring these on the Pico; common.h leans on them
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef long BaseType_t;
typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))

// The FreeRTOS heap, the storage's (common.c; ADR-017, gate 1): the PC's own
#define pvPortMalloc(size) malloc(size)
#define vPortFree(block) free(block)

#endif
