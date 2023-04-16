
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <time.h>
#include <math.h>
#include <task.h>
#include <stdlib.h>

#include "hardware/uart.h"
#include "configuration.h"
#include "scale.h"


typedef union {
    struct __attribute__((__packed__)){
        char header[2];
        char comma;
        char data[9];
        char unit[3];
        char terminator[2];
    };
    char packet[17];
} scale_standard_data_format_t;



// Statics (to be shared between multiple tasks)
static float current_scale_measurement = NAN;
SemaphoreHandle_t scale_measurement_ready;


float _decode_measurement_msg(scale_standard_data_format_t * msg) {
    // Decode header
    // Doesn't really matter though..

    // Decode weight information
    float weight = strtof(msg->data, NULL);

    return weight;
}

bool scale_init() {
    uart_init(SCALE_UART, SCALE_UART_BAUDRATE);

    gpio_set_function(SCALE_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(SCALE_UART_RX, GPIO_FUNC_UART);

    scale_measurement_ready = xSemaphoreCreateBinary();

    return true;
}


void scale_task(void *p) {
    char string_buf[20];
    uint8_t string_buf_idx = 0;

    while (true) {
        // Read all data 
        while (uart_is_readable(SCALE_UART)) {
            char ch = uart_getc(SCALE_UART);

            string_buf[string_buf_idx++] = ch;

            // If we have received 17 bytes then we can decode the message
            if (string_buf_idx == 17) {
                // Data is ready, send to decode
                current_scale_measurement = _decode_measurement_msg((scale_standard_data_format_t *) string_buf);

                // Signal the data is ready
                xSemaphoreGive(scale_measurement_ready);

                // Reset
                string_buf_idx = 0;
            }

            // \n is the terminator. We shall reset the receive of message on receiving any of those character.
            if (ch =='\n') {
                string_buf_idx = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


float scale_get_current_measurement() {
    return current_scale_measurement;
}

float scale_block_wait_for_next_measurement() {
    // You can only call this once the scheduler starts
    xSemaphoreTake(scale_measurement_ready, portMAX_DELAY);
    return scale_get_current_measurement();
}
