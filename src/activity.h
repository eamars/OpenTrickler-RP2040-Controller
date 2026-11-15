#ifndef ACTIVITY_H_
#define ACTIVITY_H_
/*
 * What the unit is doing (ADR-003, gate 3): the job that runs, one at a time, and whether a throw is under way, from
 * the start of its zeroing to its verdict (api_v1.md, `in_throw`). Each is marked under one lock, which a settings
 * write also holds while it checks and writes: so no throw starts half way through a write, and no write is checked
 * against a job that has just started.
 */
#include <stdbool.h>

typedef enum {
    ACTIVITY_NONE = 0,
    ACTIVITY_CHARGE,
    ACTIVITY_CLEANUP,
} activity_t;

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

// The job's start and end, by its mode
void activity_job_begin(activity_t job);
void activity_job_end(void);
// The throw's: its zeroing's start, and its verdict or the charge's end. Each may be said twice
void activity_throw_begin(void);
void activity_throw_end(void);

activity_t activity_get(void);
bool activity_in_throw(void);
// "none", "charge", "cleanup": the API's names (api_v1.md, `activity`)
const char *activity_name(activity_t activity);

#ifdef __cplusplus
}
#endif

#endif  // ACTIVITY_H_
