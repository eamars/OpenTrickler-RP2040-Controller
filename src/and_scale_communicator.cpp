
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include "math.h"



// Statics (to be shared between multiple tasks)
float current_scale_measurement = NAN;
SemaphoreHandle_t scale_measurement_ready;



void scale_measurement_init() {
    scale_measurement_ready = xSemaphoreCreateBinary();
}


void scale_measurement_generator(void *p) {
    current_scale_measurement = 0;
    srand(time(NULL));
    while (true) {
        xSemaphoreGive(scale_measurement_ready);

        current_scale_measurement = (rand() % 9999) / 100.0f;

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}