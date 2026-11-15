// C implemenation of button interface functions
#include <FreeRTOS.h>
#include <queue.h>

#include "mini_12864_module.h"


extern QueueHandle_t encoder_event_queue;


static ButtonEncoderEvent_t button_wait_for_input_ticks(TickType_t delay_ticks) {
    // Need to check if the queue has been initialized
    if (encoder_event_queue == NULL) {
        return BUTTON_NO_EVENT;
    }

    ButtonEncoderEvent_t button_encoder_event;
    if (!xQueueReceive(encoder_event_queue, &button_encoder_event, delay_ticks)){
        button_encoder_event = BUTTON_NO_EVENT;
    }

    return button_encoder_event;
}


ButtonEncoderEvent_t button_wait_for_input(bool block) {
    return button_wait_for_input_ticks(block ? portMAX_DELAY : 0);
}


ButtonEncoderEvent_t button_wait_for_input_ms(uint32_t timeout_ms) {
    return button_wait_for_input_ticks(pdMS_TO_TICKS(timeout_ms));
}
