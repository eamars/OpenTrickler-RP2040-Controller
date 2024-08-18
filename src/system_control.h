#ifndef SYSTEM_CONTROL_H_
#define SYSTEM_CONTROL_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "http_rest.h"

#ifdef __cplusplus
extern "C" {
#endif


bool http_rest_system_control(struct fs_file *file, int num_params, char *params[], char *values[]);
int software_reboot(void);


#ifdef __cplusplus
}
#endif


#endif  // 