#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <u8g2.h>
#include "http_rest.h"

#ifdef __cplusplus
extern "C" {
#endif

u8g2_t *get_display_handler(void);

// REST
bool http_get_display_buffer(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif


#endif  // DISPLAY_H_