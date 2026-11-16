#ifndef JOBS_H_
#define JOBS_H_

#include <stdbool.h>
#include <stdint.h>

#include "activity.h"

/*
    The job task (ADR-010, gate 2): the charge, a cleanup and an update run here, not in the screen's task, so the
    screen keeps answering while a job runs. A job reads no button: the screen turns a press into a command (a stop, a
    zero, a speed), the same entry points the API calls, by button.
*/

// What a job's end leaves the screen to show: the main menu, or the motor drivers' fault (a start refused)
#define JOBS_EXIT_MAIN_MENU 1
#define JOBS_EXIT_MOTOR_FAULT 0

#ifdef __cplusplus
extern "C" {
#endif

// Creates the job task. Before the menu's task
bool jobs_init(void);

// The screen's task: begin a job in the job task. False when one runs already
bool jobs_begin(activity_t job);

// The screen's task: true once the job begun has ended, with what the screen shows next
bool jobs_take_end(uint8_t *exit_form);

// Any task: wake the job that waits in jobs_wait(), as when another task ends it
void jobs_wake(void);

// The job: wait this long, or until woken by jobs_wake()
void jobs_wait(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif  // JOBS_H_
