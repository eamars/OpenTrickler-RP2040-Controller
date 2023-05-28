#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdint.h>
#include "http_rest.h"

#define EEPROM_WIRELESS_CONFIG_METADATA_REV                     1              // 16 byte 


typedef struct {
    uint16_t wireless_data_rev;
    char ssid[32];
    char pw[64];
    uint32_t auth;
    uint32_t timeout_ms;
    bool configured;
} __attribute__((packed)) eeprom_wireless_metadata_t;


#ifdef __cplusplus
extern "C" {
#endif


void wireless_task(void *);
bool wireless_config_init(void);
bool wireless_config_save();
uint8_t wireless_view_wifi_info(void);

bool http_rest_wireless_config(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_H_