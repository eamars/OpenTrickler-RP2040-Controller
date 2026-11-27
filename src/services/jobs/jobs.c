#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "jobs.h"

// As the menu's task had for the modes, which ran in it before (ADR-010, gate 2). On the Pico W a charge used 984
// bytes of it, an update's download over HTTPS with its check 2,952 (gate 5, progress.md)
#define JOBS_TASK_STACK_WORDS 1024
#define JOBS_TASK_PRIORITY 6

static QueueHandle_t jobs_queue = NULL;
static TaskHandle_t jobs_task_handle = NULL;
static volatile bool jobs_running = false;
static volatile bool jobs_ended = false;
static volatile uint8_t jobs_exit_form = JOBS_EXIT_MAIN_MENU;

// What runs for each job, by its activity; NULL for an activity that is no job, or whose module the build leaves out
#define JOBS_KINDS (ACTIVITY_CALIBRATION + 1)
static uint8_t (*jobs_run[JOBS_KINDS])(void);


void jobs_register(activity_t job, uint8_t (*run)(void)) {
    if ((unsigned) job < JOBS_KINDS) {
        jobs_run[job] = run;
    }
}


static void jobs_task(void *p) {
    (void) p;
    while (true) {
        activity_t job;
        xQueueReceive(jobs_queue, &job, portMAX_DELAY);

        uint8_t exit_form = JOBS_EXIT_MAIN_MENU;
        if ((unsigned) job < JOBS_KINDS && jobs_run[job] != NULL) {
            exit_form = jobs_run[job]();
        }
        else {
            printf("Jobs: %s is no job\n", activity_name(job));
        }

        // Not running before ended: the menu, once it sees the end, may begin the next job at once
        jobs_exit_form = exit_form;
        jobs_running = false;
        jobs_ended = true;
        // The screen looks at once, not at its next wake
        activity_wake_screen();
    }
}


bool jobs_init(void) {
    jobs_queue = xQueueCreate(1, sizeof(activity_t));
    return jobs_queue != NULL &&
           xTaskCreate(jobs_task, "Jobs", JOBS_TASK_STACK_WORDS, NULL, JOBS_TASK_PRIORITY, &jobs_task_handle) == pdPASS;
}


void jobs_wake(void) {
    if (jobs_task_handle != NULL) {
        xTaskNotifyGive(jobs_task_handle);
    }
}


void jobs_wait(uint32_t ms) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}


bool jobs_begin(activity_t job) {
    if (jobs_running) {
        return false;
    }
    jobs_running = true;
    jobs_ended = false;
    return xQueueSend(jobs_queue, &job, 0) == pdTRUE;
}


bool jobs_take_end(uint8_t *exit_form) {
    if (!jobs_ended) {
        return false;
    }
    jobs_ended = false;
    *exit_form = jobs_exit_form;
    return true;
}
