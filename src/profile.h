#ifndef PROFILE_H_
#define PROFILE_H_

#include <stdint.h>
#include <stdbool.h>
#include "http_rest.h"


#define PROFILE_NAME_MAX_LEN    16
#define MAX_PROFILE_CNT         8

#define EEPROM_PROFILE_DATA_REV             2           // 16 bit

typedef struct __attribute__((packed))  // The alignment is critical to the CRC calculation
{  
    uint32_t rev;
    uint32_t compatibility;
    
    char name[PROFILE_NAME_MAX_LEN];

    float coarse_kp;
    float coarse_ki;
    float coarse_kd;

    float fine_kp;
    float fine_ki;
    float fine_kd;

    float min_flow_speed_rps;
    float max_flow_speed_rps;

    uint8_t crc8;
} profile_t;


typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;

    profile_t profiles[MAX_PROFILE_CNT];
} eeprom_profile_data_t;


#ifdef __cplusplus
extern "C" {
#endif



// Interface
bool profile_data_init(void);
bool profile_data_save();

profile_t * profile_select(uint8_t idx);

void profile_update_checksum();

profile_t * get_selected_profile();

// REST interface
bool http_rest_profile_config(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}
#endif

#endif  // PROFILE_H_