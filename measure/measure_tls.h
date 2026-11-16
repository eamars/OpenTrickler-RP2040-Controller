#ifndef MEASURE_TLS_H_
#define MEASURE_TLS_H_

#include <stdbool.h>
#include <stddef.h>

#include "remote_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The console's `tls` (ADR-010, gate 1, measure build only): `tls get <url>` starts one fetch, over HTTPS or HTTP,
 * in a task of its own; `tls` reports how it went and what it took.
 */
bool measure_tls_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                         void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // MEASURE_TLS_H_
