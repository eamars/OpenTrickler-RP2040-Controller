#ifndef CLEANUP_MODE_H_
#define CLENAUP_MODE_H_

#include <stdint.h>
#include "http_rest.h"
#include "motors.h"


typedef enum {
    CLEANUP_MODE_EXIT = 0,
    CLEANUP_MODE_ENTER = 1,
} cleanup_mode_state_t;


typedef struct {
    float trickler_speed;
    cleanup_mode_state_t cleanup_mode_state;
} cleanup_mode_config_t;


// C Functions
#ifdef __cplusplus
extern "C" {
#endif


uint8_t cleanup_mode_menu();

bool http_rest_cleanup_mode_state(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}  // __cplusplus
#endif

#endif  // CLEANUP_MODE_H_