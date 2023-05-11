#ifndef SCALE_H_
#define SCALE_H_

#include <stdint.h>
#include "app.h"

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

bool scale_config_save(void);


// Key bindings
void scale_press_re_zero_key();
void scale_press_print_key();
void scale_press_sample_key();
void scale_press_mode_key();
void scale_press_cal_key();
void scale_press_on_off_key();
void scale_display_off();
void scale_display_on();

// Features
uint8_t scale_calibrate_with_external_weight();
AppState_t scale_enable_fast_report(AppState_t prev_state);


#ifdef __cplusplus
}
#endif


#endif