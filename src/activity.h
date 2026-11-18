#ifndef ACTIVITY_H_
#define ACTIVITY_H_
/*
 * What the unit is doing (ADR-003, gates 3 and 4): the job that runs, one at a time, and whether a throw is under
 * way, from the start of its zeroing to its verdict (api_v1.md, `in_throw`). Each is marked under one lock, which a
 * settings write also holds while it checks and writes: so no throw starts half way through a write, and no write
 * is checked against a job that has just started.
 *
 * A job is started from outside the menu (the API, the console) by a request the menu takes when it next looks, and
 * stopped by a request its mode takes where it reads the reset button. Neither waits for the other task, and a stop
 * goes through no queue that could be full: it never fails (api_v1.md).
 */
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACTIVITY_NONE = 0,
    ACTIVITY_CHARGE,
    ACTIVITY_CLEANUP,
    ACTIVITY_VIEW,              // never set since ADR-010: kept so the API's v1 names stay as they were
    ACTIVITY_UPDATE,            // a firmware update (ADR-004): an image written into the free slot
} activity_t;

// Who asked for a start or a stop (api_v1.md, the events' `by`)
typedef enum {
    ACTIVITY_BY_BUTTON = 0,
    ACTIVITY_BY_API,
    ACTIVITY_BY_CONSOLE,
    ACTIVITY_BY_UNIT,           // the unit itself: a fault, a cleanup's time run out
} activity_by_t;

// A start the menu has not taken in this long is dropped, so it never starts a job later
#define ACTIVITY_REQUEST_MAX_MS 1500

#ifdef __cplusplus
extern "C" {
#endif

// Before any task that marks or locks
void activity_init(void);

// Held for a check and a write; never across a wait for anything but the lock
void activity_lock(void);
void activity_unlock(void);
// The lock if it is free within that many ms; false when it is not
bool activity_try_lock(unsigned wait_ms);

/*
 * A start asked from outside the menu: refused (false) when a job runs or another start is asked already. The
 * caller wakes the menu (menu_wake()); the menu takes the request with activity_take_start().
 */
bool activity_request_start(activity_t job, activity_by_t by);
// The menu: a start asked and still fresh, taken (once); false when there is none
bool activity_take_start(activity_t *job);
// The menu, after the mode it ran returns: a request its mode did not begin (a driver not OK) is done with
void activity_clear_start(void);
// Whether a start is asked and to come (`starting`)
bool activity_start_pending(activity_t *job);

// The job's start and end, by its mode. Its `by` is the request's when the menu took one, else the button's
void activity_job_begin(activity_t job);
void activity_job_end(void);
activity_by_t activity_job_by(void);

/*
 * A stop asked from outside: true when there was something to stop (the job, if `job` is ACTIVITY_NONE or matches,
 * or a start not yet taken). The mode takes it where it reads the reset button (activity_take_stop()).
 */
bool activity_request_stop(activity_t job, activity_by_t by);
bool activity_take_stop(activity_by_t *by);

// The throw's: its zeroing's start, and its verdict or the charge's end. Each may be said twice
void activity_throw_begin(void);
void activity_throw_end(void);

activity_t activity_get(void);
// A job runs: the charge, a cleanup, an update. A view is none; since ADR-010 nothing sets it (a job takes the
// screen from any page), and it stays a name of the API's v1
bool activity_job_runs(void);
bool activity_in_throw(void);
// "none", "charge", "purge", "view", "update": the API's names (api_v1.md, `activity`)
const char *activity_name(activity_t activity);
// "button", "api", "console", "unit"
const char *activity_by_name(activity_by_t by);

#ifdef __cplusplus
}
#endif

#endif  // ACTIVITY_H_
