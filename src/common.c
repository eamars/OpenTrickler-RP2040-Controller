#include <FreeRTOS.h>
#include <task.h>
#include "common.h"
#include "pico/time.h"


void delay_ms(uint32_t ms, BaseType_t scheduler_state) {
    if (scheduler_state == taskSCHEDULER_RUNNING){
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else {
        // sleep_ms(arg_int);
        busy_wait_us(ms * 1000ULL);
    }
}

const char * true_string = "true";
const char * false_string = "false";
const char * boolean_string(bool var) {
    return var ? true_string : false_string;
}