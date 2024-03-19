#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "pico/time.h"


const char * http_json_header = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";


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
const char * boolean_to_string(bool var) {
    return var ? true_string : false_string;
}

bool string_to_boolean(char * s) {
    bool var = false;

    if (strcmp(s, true_string) == 0) {
        var = true;
    }

    return var;
}


int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places) {
    int return_value = 0;
    
    switch (decimal_places) {
        case DP_2:
            return_value = sprintf(output_decimal_str, "%0.02f", var);
            break;
        case DP_3:
            return_value = sprintf(output_decimal_str, "%0.03f", var);
            break;
        default:
            break;
    }
    
    return return_value;
}