#ifndef RC_CONSOLE_H_
#define RC_CONSOLE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the task that reads command lines from the console (stdio, which is RTT through the debug probe)
 * and prints each reply (ADR-001, step 3). Call after stdio_init_all(), before the scheduler starts.
 */
bool rc_console_init(void);

#ifdef __cplusplus
}
#endif

#endif  // RC_CONSOLE_H_
