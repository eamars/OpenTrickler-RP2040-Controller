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

// "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
extern const char * http_json_header;

typedef struct {
    PIO pio;
    int sm;
} pio_config_t;


/**
 * If the RTOS is running then use RTOS delay. Otherwise use dummy delay. 
*/
void delay_ms(uint32_t ms, BaseType_t scheduler_state);

const char * boolean_to_string(bool var);
bool string_to_boolean(char * s);

int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places);

uint32_t software_crc32(void * data, size_t length);

/** 
 * @brief Load configuration from persistent storage. 
 */
bool load_config(uint16_t addr, void * cfg, const void * default_cfg, size_t size, uint16_t rev_validation);

/**
 * @brief Save configuration to persistent storage
 */
bool save_config(uint16_t addr, void * cfg, size_t size);

#ifdef __cplusplus
}
#endif


#endif // COMMON_H_