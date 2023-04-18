#ifndef SCALE_H_
#define SCALE_H_

#include <stdint.h>

#define EEPROM_SCALE_DATA_REV                     1              // 16 byte 


typedef enum {
    SCALE_UNIT_GRAIN = 0,
    SCALE_UNIT_GRAM = 1,
} scale_unit_t;

typedef struct {
    uint16_t scale_data_rev;
    scale_unit_t scale_unit;
} __attribute__((packed)) eeprom_scale_data_t;




#ifdef __cplusplus
extern "C" {
#endif

// Measurement related calls
bool scale_init();
void scale_listener_task(void *p);
float scale_get_current_measurement();
float scale_block_wait_for_next_measurement();



#ifdef __cplusplus
}
#endif


#endif