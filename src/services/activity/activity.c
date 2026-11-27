/*
 * What the unit is doing (ADR-003, gates 3 and 4); see activity.h.
 */
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "activity.h"


static SemaphoreHandle_t activity_mutex = NULL;
static volatile activity_t activity_now = ACTIVITY_NONE;
static volatile activity_by_t activity_now_by = ACTIVITY_BY_BUTTON;
static volatile bool activity_throw = false;

// A start asked, when, and whether the menu has taken it; ACTIVITY_NONE when there is none
static volatile activity_t activity_asked = ACTIVITY_NONE;
static volatile activity_by_t activity_asked_by = ACTIVITY_BY_BUTTON;
static volatile TickType_t activity_asked_tick = 0;
static volatile bool activity_asked_taken = false;

// A stop asked and not yet taken
static volatile bool activity_stop_asked = false;
static volatile activity_by_t activity_stop_by = ACTIVITY_BY_BUTTON;

// The screen's two calls, handed over by the UI; NULL until then
static const activity_screen_t *activity_screen = NULL;


void activity_set_screen(const activity_screen_t *screen) {
    activity_screen = screen;
}


void activity_wake_screen(void) {
    if (activity_screen != NULL) {
        activity_screen->wake();
    }
}


bool activity_screen_takes_starts(void) {
    return activity_screen != NULL && activity_screen->takes_starts();
}


/*
    Recursive: a task that holds the lock may take it again. The API's zero reads the charge's state while it holds
    the lock, and that read takes the lock too: with a plain mutex the server's task waited on itself for good
*/
void activity_init(void) {
    if (activity_mutex == NULL) {
        activity_mutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(activity_mutex != NULL);
    }
}


void activity_lock(void) {
    xSemaphoreTakeRecursive(activity_mutex, portMAX_DELAY);
}


void activity_unlock(void) {
    xSemaphoreGiveRecursive(activity_mutex);
}


bool activity_try_lock(unsigned wait_ms) {
    return xSemaphoreTakeRecursive(activity_mutex, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}


// Under the lock: a start asked and still to come, taken by the menu or fresh enough to be
static bool start_pending(void) {
    if (activity_asked == ACTIVITY_NONE) {
        return false;
    }
    if (activity_asked_taken) {
        return true;
    }
    if ((xTaskGetTickCount() - activity_asked_tick) * portTICK_PERIOD_MS < ACTIVITY_REQUEST_MAX_MS) {
        return true;
    }
    activity_asked = ACTIVITY_NONE;     // gone stale: dropped, never started later
    return false;
}


bool activity_request_start(activity_t job, activity_by_t by) {
    activity_lock();
    bool asked = activity_now == ACTIVITY_NONE && !start_pending();
    if (asked) {
        activity_asked = job;
        activity_asked_by = by;
        activity_asked_tick = xTaskGetTickCount();
        activity_asked_taken = false;
    }
    activity_unlock();
    return asked;
}


bool activity_take_start(activity_t *job) {
    activity_lock();
    bool take = start_pending() && !activity_asked_taken;
    *job = take ? activity_asked : ACTIVITY_NONE;
    if (take) {
        activity_asked_taken = true;    // taken once: its mode begins it, or the menu clears it after
    }
    activity_unlock();
    return take;
}


void activity_clear_start(void) {
    activity_lock();
    activity_asked = ACTIVITY_NONE;
    // A stop asked for that request, taken and then never begun (a driver not OK), goes with it: kept, it ended the
    // next job at its start (review of ADR-010's gate 4)
    if (activity_now == ACTIVITY_NONE) {
        activity_stop_asked = false;
    }
    activity_unlock();
}


bool activity_start_pending(activity_t *job) {
    activity_lock();
    bool pending = start_pending();
    *job = pending ? activity_asked : ACTIVITY_NONE;
    activity_unlock();
    return pending;
}


void activity_job_begin(activity_t job) {
    activity_lock();
    // The menu took a request for this job: it is the requester's. Else a person at the unit started it, and a
    // request still waiting is overtaken: dropped
    bool from_request = activity_asked == job && activity_asked_taken;
    activity_now_by = from_request ? activity_asked_by : ACTIVITY_BY_BUTTON;
    activity_asked = ACTIVITY_NONE;
    activity_now = job;
    if (!from_request) {
        activity_stop_asked = false;    // a stop asked for the request before it began is kept
    }
    activity_unlock();
}


void activity_job_end(void) {
    activity_lock();
    activity_now = ACTIVITY_NONE;
    activity_throw = false;
    activity_stop_asked = false;
    activity_unlock();
}


activity_by_t activity_job_by(void) {
    return activity_now_by;
}


bool activity_request_stop(activity_t job, activity_by_t by) {
    activity_lock();
    bool stopped = false;
    if (start_pending() && (job == ACTIVITY_NONE || job == activity_asked)) {
        if (activity_asked_taken) {
            // Taken, its mode about to begin: stopped as soon as it does
            activity_stop_asked = true;
            activity_stop_by = by;
        }
        else {
            activity_asked = ACTIVITY_NONE;     // never taken
        }
        stopped = true;
    }
    // The charge, the cleanup and an update take a stop, where they read the reset button. A view is a
    // person's at the unit: stopped=false, rather than a stop said and not done (review C1)
    bool takes_stop = activity_now == ACTIVITY_CHARGE || activity_now == ACTIVITY_CLEANUP ||
                      activity_now == ACTIVITY_UPDATE || activity_now == ACTIVITY_CALIBRATION;
    if (takes_stop && (job == ACTIVITY_NONE || job == activity_now)) {
        activity_stop_asked = true;
        activity_stop_by = by;
        stopped = true;
    }
    activity_unlock();
    return stopped;
}


bool activity_take_stop(activity_by_t *by) {
    if (!activity_stop_asked) {
        return false;   // read without the lock: the common case, every loop of a mode
    }
    activity_lock();
    bool asked = activity_stop_asked;
    activity_stop_asked = false;
    if (by != NULL) {
        *by = activity_stop_by;
    }
    activity_unlock();
    return asked;
}


void activity_throw_begin(void) {
    activity_lock();
    activity_throw = true;
    activity_unlock();
}


void activity_throw_end(void) {
    activity_lock();
    activity_throw = false;
    activity_unlock();
}


activity_t activity_get(void) {
    return activity_now;
}


bool activity_job_runs(void) {
    activity_t now = activity_now;
    return now == ACTIVITY_CHARGE || now == ACTIVITY_CLEANUP || now == ACTIVITY_UPDATE ||
           now == ACTIVITY_CALIBRATION;
}


bool activity_in_throw(void) {
    return activity_throw;
}


const char *activity_name(activity_t activity) {
    switch (activity) {
        case ACTIVITY_CHARGE:
            return "charge";
        case ACTIVITY_CLEANUP:
            return "purge";
        case ACTIVITY_VIEW:
            return "view";
        case ACTIVITY_UPDATE:
            return "update";
        case ACTIVITY_CALIBRATION:
            return "calibration";
        case ACTIVITY_NONE:
        default:
            return "none";
    }
}


const char *activity_by_name(activity_by_t by) {
    switch (by) {
        case ACTIVITY_BY_API:
            return "api";
        case ACTIVITY_BY_CONSOLE:
            return "console";
        case ACTIVITY_BY_UNIT:
            return "unit";
        case ACTIVITY_BY_BUTTON:
        default:
            return "button";
    }
}
