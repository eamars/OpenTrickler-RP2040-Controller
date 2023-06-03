#include <FreeRTOS.h>
#include <queue.h>

#include "rest_app_control.h"


QueueHandle_t rest_event_queue = NULL;


void rest_app_control_init() {
    rest_event_queue = xQueueCreate(1, sizeof(rest_control_event_t));

    if (rest_event_queue == NULL) {
        assert(false);
    }
}

