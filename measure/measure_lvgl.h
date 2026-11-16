#ifndef MEASURE_LVGL_H_
#define MEASURE_LVGL_H_

#include <stdbool.h>
#include <stddef.h>

#include "remote_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The console's `lvgl` (ADR-010, gate 1, measure build only): LVGL started beside u8g2, drawing a menu page into a
 * frame of its own; `lvgl` reports, `lvgl next` moves the focus, `lvgl frame` prints the frame.
 */
bool measure_lvgl_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                          void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // MEASURE_LVGL_H_
