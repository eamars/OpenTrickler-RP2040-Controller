#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdbool.h>
#include <stdint.h>

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

// The network the unit is on, as GET /api/v1/network gives it (ADR-003, gate 4)
typedef struct {
    const char *mode;       // "station", "access_point", "none"
    char ssid[33];
    char address[24];       // "" with none
    const char *hostname;   // the mDNS name, and the access point's
    bool has_rssi;
    int32_t rssi;           // dBm, as a station
    const char *trouble;    // as a station not joined, why its last join failed: "Wrong key", "Not found",
                            // "Join failed"; NULL when joined or none was tried
} wireless_info_t;
void wireless_get_info(wireless_info_t *info);
// The same without the signal, which asks the radio under lwIP's lock: what the screen reads at every refresh
void wireless_get_link(wireless_info_t *info);

// The station leaves its network, as when its router goes away: the console's `wifi leave` (ADR-004)
bool wireless_station_leave(void);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_H_