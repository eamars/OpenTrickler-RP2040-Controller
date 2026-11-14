/*
 * The remote control's transport (ADR-001, step 3): reads command lines from the console and prints each reply
 * with one printf. The console is stdio, which this build sends over RTT through the debug probe; this file does
 * not know that, so another stdio driver needs no change here.
 */
#include <stdio.h>
#include <assert.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/stdlib.h"

#include "rc_console.h"
#include "remote_control.h"

#if LIB_PICO_STDIO_RTT
#include <string.h>
#include "SEGGER_RTT.h"

// How long a command's line waits for room in the RTT buffer before it is printed anyway
#define RC_CONSOLE_WRITE_WAIT_MS 500
#endif


// Large enough for `state`
#define RC_CONSOLE_REPLY_SIZE 512

// RTT has no interrupt, so the console is polled. A line of 128 characters arrives within a few polls
#define RC_CONSOLE_POLL_MS 20

#define RC_CONSOLE_TASK_STACK 1024      // words
#define RC_CONSOLE_TASK_PRIORITY 2      // below the menu (6) and the modes

// A test build (-DRC_TEST_FAULT=1 or 2, see CMakeLists.txt) faults this long after start-up
#define RC_TEST_FAULT_DELAY_MS 10000


static char rc_console_line[REMOTE_CONTROL_LINE_MAX + 1];
static char rc_console_reply[RC_CONSOLE_REPLY_SIZE];


// A command's lines before its reply, on the console as they come
static void rc_console_write_line(const char *line, void *ctx) {
#if LIB_PICO_STDIO_RTT
    // RTT drops a line that does not fit its buffer. Wait for the probe to make room, a little, so a snap's
    // lines all arrive; if it never does (no probe reading), the line is dropped as any log line would be
    for (int waited_ms = 0; waited_ms < RC_CONSOLE_WRITE_WAIT_MS; waited_ms += RC_CONSOLE_POLL_MS) {
        if (SEGGER_RTT_GetAvailWriteSpace(0) > strlen(line) + 1) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(RC_CONSOLE_POLL_MS));
    }
#endif
    printf("%s\n", line);
}


static void rc_console_print_reply(void) {
    // One printf per reply, so the line is not interleaved with log output
    printf("%s\n", rc_console_reply);
}


static void rc_console_task(void *p) {
    size_t line_len = 0;
    bool discarding = false;  // dropping the rest of an over-long line until its line ending

    // Never run what was typed before the task started
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {
    }

    printf("@hello remote control ready\n");

    while (true) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            vTaskDelay(pdMS_TO_TICKS(RC_CONSOLE_POLL_MS));
            continue;
        }

        // CR, LF and CRLF all end a line; the empty line between CR and LF is ignored
        if (c == '\r' || c == '\n') {
            if (!discarding && line_len > 0) {
                rc_console_line[line_len] = '\0';
                remote_control_execute(rc_console_line, rc_console_reply, sizeof(rc_console_reply),
                                       rc_console_write_line, NULL);
                rc_console_print_reply();
                remote_control_after_reply();
            }
            line_len = 0;
            discarding = false;
            continue;
        }

        if (discarding) {
            continue;
        }

        if (line_len == REMOTE_CONTROL_LINE_MAX) {
            discarding = true;
            line_len = 0;
            snprintf(rc_console_reply, sizeof(rc_console_reply), "@err too long");
            rc_console_print_reply();
            continue;
        }

        rc_console_line[line_len++] = (char) c;
    }
}


#ifdef RC_TEST_FAULT
// Test builds only: proves what an assert or a hard fault leaves behind, and that the probe brings the unit back
static void rc_test_fault_task(void *p) {
    vTaskDelay(pdMS_TO_TICKS(RC_TEST_FAULT_DELAY_MS));
#if RC_TEST_FAULT == 1
    printf("rc_test_fault: assert now\n");
    assert(false);
#elif RC_TEST_FAULT == 2
    printf("rc_test_fault: hard fault now\n");
    void (*null_fn)(void) = NULL;
    null_fn();
#endif
    vTaskDelete(NULL);
}
#endif


bool rc_console_init(void) {
    BaseType_t rtos_return = xTaskCreate(rc_console_task, "RC Console", RC_CONSOLE_TASK_STACK, NULL,
                                         RC_CONSOLE_TASK_PRIORITY, NULL);
    if (rtos_return != pdPASS) {
        printf("Unable to create the remote control's console task\n");
        return false;
    }

#ifdef RC_TEST_FAULT
    xTaskCreate(rc_test_fault_task, "RC Test Fault", 512, NULL, RC_CONSOLE_TASK_PRIORITY, NULL);
#endif

    return true;
}
