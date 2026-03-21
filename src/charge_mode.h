#ifndef CHARGE_MODE_H_
#define CHARGE_MODE_H_

#include <stdint.h>
#include "http_rest.h"
#include "common.h"
#include "neopixel_led.h"


#define EEPROM_CHARGE_MODE_DATA_REV                     13             // Added scale stabilization config

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

    // AI tuning time targets
    uint32_t coarse_time_target_ms;
    uint32_t total_time_target_ms;

    // ML data collection during normal (non-tuning) charges
    bool ml_data_collection_enabled;

    // Auto zero scale when cup is returned
    bool auto_zero_on_cup_return;

    // Pulse mode - helps with slow scales near target
    bool pulse_mode_enabled;
    float pulse_threshold;          // Start pulsing when error < this (grains)
    uint32_t pulse_duration_ms;     // Motor on time per pulse
    uint32_t pulse_wait_ms;         // Wait time between pulses for scale to update

    // Scale stabilization after motors stop (before overthrow/underthrow decision)
    bool stabilization_enabled;     // true = fixed wait, false = adaptive SD-based
    uint32_t stabilization_time_ms; // Fixed wait time when enabled (default 2000ms)

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


// C Functions
#ifdef __cplusplus
extern "C" {
#endif


bool charge_mode_config_init(void);
uint8_t charge_mode_menu(bool charge_mode_skip_user_input);
bool charge_mode_config_save(void);

// REST interface
bool http_rest_charge_mode_config(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_charge_mode_state(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CHARGE_MODE_H_