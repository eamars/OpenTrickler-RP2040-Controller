#ifndef CHARGE_MODE_H_
#define CHARGE_MODE_H_

#include <stdint.h>
#include "http_rest.h"
#include "common.h"
#include "neopixel_led.h"


#define EEPROM_CHARGE_MODE_DATA_REV                     8              // 16 byte 

#define WEIGHT_STRING_LEN 8

typedef enum {
    CHARGE_MODE_EXIT = 0,
    CHARGE_MODE_WAIT_FOR_ZERO = 1,
    CHARGE_MODE_WAIT_FOR_COMPLETE = 2,
    CHARGE_MODE_WAIT_FOR_CUP_REMOVAL = 3,
    CHARGE_MODE_WAIT_FOR_CUP_RETURN = 4,
} charge_mode_state_t;

typedef struct {
    uint16_t charge_mode_data_rev;

    float coarse_stop_threshold;
    float fine_stop_threshold;

    float set_point_sd_margin;
    float set_point_mean_margin;
    float coarse_stop_gate_ratio; // 0.0=open, 1.0=close, -1.0=disabled (optional)

    decimal_places_t decimal_places;

    // Precharge
    bool precharge_enable;
    uint32_t precharge_time_ms;
    float precharge_speed_rps;

    // LED related settings
    rgbw_u32_t neopixel_normal_charge_colour;
    rgbw_u32_t neopixel_under_charge_colour;
    rgbw_u32_t neopixel_over_charge_colour;
    rgbw_u32_t neopixel_not_ready_colour;

} eeprom_charge_mode_data_t;

typedef struct {
    eeprom_charge_mode_data_t eeprom_charge_mode_data;
    float target_charge_weight;
    uint32_t charge_mode_event;
    charge_mode_state_t charge_mode_state;
} charge_mode_config_t;


bool charge_mode_config_init(void);
uint8_t charge_mode_menu(bool charge_mode_skip_user_input);

// C Functions
#ifdef __cplusplus
extern "C" {
#endif

bool charge_mode_config_save(void);

// REST interface
bool http_rest_charge_mode_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_charge_mode_state(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CHARGE_MODE_H_