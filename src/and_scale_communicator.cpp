
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include "math.h"


// Statics (to be shared between multiple tasks)
// QueueHandle_t scale_measurement_queue = NULL;
float current_scale_measurement = NAN;


void scale_measurement_init() {
    // scale_measurement_queue = xQueueCreate(2, sizeof(float));
}


void scale_measurement_generator(void *p) {
    current_scale_measurement = 0;
    while (true) {
        // float * real_cnt = (float *) malloc(sizeof(float));
        // *real_cnt = cnt / 10.0f;
        // xQueueSend(scale_measurement_queue, real_cnt, portMAX_DELAY);

        // cnt += 1;
        current_scale_measurement += 0.01;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}