#ifndef CHARGE_MODE_H_
#define CHARGE_MODE_H_

#include <stdint.h>

#define EEPROM_CHARGE_MODE_DATA_REV                     3              // 16 byte 


typedef struct {
    uint16_t charge_mode_data_rev;
    float coarse_kp;
    float coarse_ki;
    float coarse_kd;

    float fine_kp;
    float fine_ki;
    float fine_kd;

    float error_margin_grain;
    float zero_sd_margin_grain;
    float zero_mean_stability_grain;

} __attribute__((packed)) eeprom_charge_mode_data_t;

typedef struct {
    eeprom_charge_mode_data_t eeprom_charge_mode_data;
} charge_mode_config_t;


bool charge_mode_config_init(void);

// C Functions
#ifdef __cplusplus
extern "C" {
#endif

bool charge_mode_config_save(void);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CHARGE_MODE_H_