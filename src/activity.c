/*
 * What the unit is doing (ADR-003, gate 3); see activity.h.
 */
#include <FreeRTOS.h>
#include <semphr.h>

#include "activity.h"


static SemaphoreHandle_t activity_mutex = NULL;
static volatile activity_t activity_now = ACTIVITY_NONE;
static volatile bool activity_throw = false;


void activity_init(void) {
    if (activity_mutex == NULL) {
        activity_mutex = xSemaphoreCreateMutex();
        configASSERT(activity_mutex != NULL);
    }
}


void activity_lock(void) {
    xSemaphoreTake(activity_mutex, portMAX_DELAY);
}


void activity_unlock(void) {
    xSemaphoreGive(activity_mutex);
}


bool activity_try_lock(unsigned wait_ms) {
    return xSemaphoreTake(activity_mutex, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}


void activity_job_begin(activity_t job) {
    activity_lock();
    activity_now = job;
    activity_unlock();
}


void activity_job_end(void) {
    activity_lock();
    activity_now = ACTIVITY_NONE;
    activity_throw = false;
    activity_unlock();
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


bool activity_in_throw(void) {
    return activity_throw;
}


const char *activity_name(activity_t activity) {
    switch (activity) {
        case ACTIVITY_CHARGE:
            return "charge";
        case ACTIVITY_CLEANUP:
            return "cleanup";
        case ACTIVITY_NONE:
        default:
            return "none";
    }
}
