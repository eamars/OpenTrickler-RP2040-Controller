#include <pico/cyw43_arch.h>
#include <FreeRTOS.h>
#include <queue.h>
#include "cyw43_control.h"

typedef enum {
    _CYW43_INIT,
    _CYW43_DEINIT,
    _LED_ON,
    _LED_OFF,
} _cyw43_control_flag_t;

static QueueHandle_t cyw43_control_queue;


void cyw43_task(void *p){
    cyw43_control_queue = xQueueCreate(1, sizeof(_cyw43_control_flag_t));

    cyw43_start(false);

    while (true){
        _cyw43_control_flag_t control;
        xQueueReceive(cyw43_control_queue, &control, portMAX_DELAY);

        switch (control) {
            case _CYW43_INIT: {
                if (cyw43_arch_init()) {
                    exit(-1);
                }
                break;
            }
            case _CYW43_DEINIT: {
                cyw43_arch_deinit();
                break;
            }
            default:
                break;
        }
    }
}


void cyw43_start(bool block) {
    if (cyw43_control_queue) {
        _cyw43_control_flag_t control = _CYW43_INIT;
        xQueueSend(cyw43_control_queue, &control, portMAX_DELAY);

        // TODO: Implement the actual delay
        if (block) {
            vTaskDelay(100);
        }
    }

}
void cyw43_stop(bool block) {
    if (cyw43_control_queue) {
        _cyw43_control_flag_t control = _CYW43_DEINIT;
        xQueueSend(cyw43_control_queue, &control, portMAX_DELAY);
    }
}
