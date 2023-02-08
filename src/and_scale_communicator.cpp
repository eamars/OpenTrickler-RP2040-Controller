
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>


// Statics (to be shared between multiple tasks)
QueueHandle_t scale_measurement_queue = NULL;


void scale_measurement_init() {
    scale_measurement_queue = xQueueCreate(2, sizeof(float));
}


void scale_measurement_generator(void *p) {
    uint32_t cnt = 0;
    while (true) {
        float * real_cnt = (float *) malloc(sizeof(float));
        *real_cnt = cnt / 10.0f;
        xQueueSend(scale_measurement_queue, real_cnt, portMAX_DELAY);

        cnt += 1;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}