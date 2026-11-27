#ifndef REMOTE_CONTROL_H_
#define REMOTE_CONTROL_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Longest command line accepted, without the line ending
#define REMOTE_CONTROL_LINE_MAX 128


/**
 * @brief How a transport takes the lines a command sends before its reply: one line, no line ending.
 * Called from the task executing the command.
 */
typedef void (*remote_control_write_fn)(const char *line, void *ctx);

/**
 * @brief Execute one command line (no line ending) and write one reply line (no line ending) into reply.
 * Lines the command sends before its reply go to `write` (NULL drops them). Knows no transport (ADR-001).
 * @return true for an `@ok` reply, false for an `@err` reply.
 */
bool remote_control_execute(const char *line, char *reply, size_t reply_size, remote_control_write_fn write,
                            void *write_ctx);

/**
 * @brief Called by the transport once the reply is out: carries out what must wait for it (the restart of `reboot`).
 */
void remote_control_after_reply(void);

#ifdef __cplusplus
}
#endif

#endif  // REMOTE_CONTROL_H_
