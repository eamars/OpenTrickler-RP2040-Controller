#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdint.h>
#include "http_rest.h"

#define EEPROM_WIRELESS_CONFIG_METADATA_REV                     2              // 16 byte 


typedef enum {
    AUTH_OPEN = 0,
    AUTH_WPA_TKIP_PSK = 1,
    AUTH_WPA2_AES_PSK = 2,
    AUTH_WPA2_MIXED_PSK = 3,
} cyw43_auth_t;


typedef struct {
    uint16_t wireless_data_rev;
    char ssid[32];
    char pw[64];
    cyw43_auth_t auth;
    uint32_t timeout_ms;
    bool enable;
} eeprom_wireless_metadata_t;





#ifdef __cplusplus
extern "C" {
#endif


void wireless_task(void *);
bool wireless_init(void);
bool wireless_config_save();
uint8_t wireless_view_wifi_info(void);

bool http_rest_wireless_config(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_H_