#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <FreeRTOS.h>

typedef enum {
    DP_2 = 0,
    DP_3 = 1,
} decimal_places_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * If the RTOS is running then use RTOS delay. Otherwise use dummy delay. 
*/
void delay_ms(uint32_t ms, BaseType_t scheduler_state);

const char * boolean_to_string(bool var);
bool string_to_boolean(char * s);

int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places);

#ifdef __cplusplus
}
#endif


#endif // COMMON_H_