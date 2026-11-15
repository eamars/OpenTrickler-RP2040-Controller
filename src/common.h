#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include "hardware/pio.h"


typedef enum {
    DP_2 = 0,
    DP_3 = 1,
} decimal_places_t;


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PIO pio;
    int sm;
} pio_config_t;


/**
 * If the RTOS is running then use RTOS delay. Otherwise use dummy delay. 
*/
void delay_ms(uint32_t ms, BaseType_t scheduler_state);

/*
    A weight as text, with the buffer's length: never written past it, and an empty string for a number of decimals
    that is neither (ADR-012; audit M5). float_to_string writes with no length and leaves the buffer as it was.
*/
int weight_to_string(char * output_decimal_str, size_t output_size, float var, decimal_places_t decimal_places);

const char * boolean_to_string(bool var);
bool string_to_boolean(char * s);

int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places);

/** 
 * @brief Load configuration from persistent storage. 
 */
bool load_config(uint16_t addr, void * cfg, const void * default_cfg, size_t size, uint16_t rev_validation);

/**
 * @brief Save configuration to persistent storage
 */
bool save_config(uint16_t addr, void * cfg, size_t size);

/**
 * @brief Whether the struct in RAM is what was last stored at that address, or read from it at the start: their
 * CRCs (ADR-003, gate 3: `saved`). So a change from the menu or the console counts as one from the web does.
 */
bool config_is_saved(uint16_t addr, const void * cfg, size_t size);

#ifdef __cplusplus
}
#endif


#endif // COMMON_H_