#ifndef CLEANUP_MODE_H_
#define CLEANUP_MODE_H_

#include <stdint.h>
#include "activity.h"
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
// The cleanup's state in its own words, as the screen shows it (ADR-010, gate 4)
const char *cleanup_mode_get_title(void);

/*
 * A cleanup asked from outside the menu, by the API or the console (ADR-003, gate 4): handed to the menu with no
 * wait, or refused. A stop is activity_request_stop(ACTIVITY_CLEANUP, by).
 */
typedef enum {
    CLEANUP_START_OK = 0,
    CLEANUP_START_BUSY,         // a job runs, a start is asked already, or a person has a mode or a view at the unit
    CLEANUP_START_DRIVER_FAULT,
} cleanup_start_result_t;
cleanup_start_result_t cleanup_mode_request_start(activity_by_t by);

/*
 * Both tricklers at this speed, within the lower of the two motors' maximum speeds; with time_s (finite), for at
 * most that long, after which the cleanup stops by itself. False when no cleanup runs.
 */
bool cleanup_mode_set_speed(float speed_rps, float time_s);

// The cleanup as GET /api/v1/cleanup gives it
typedef struct {
    const char *state;      // idle, starting, running, fault
    const char *by;         // who started it (button, api, console); NULL with none
    float speed_rps;
    float time_left_s;      // NAN: a speed with no time
} cleanup_status_t;
void cleanup_mode_get_status(cleanup_status_t *status);

#ifdef __cplusplus
}  // __cplusplus
#endif

#endif  // CLEANUP_MODE_H_
