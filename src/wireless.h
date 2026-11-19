#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

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
    // The set-up finished or skipped (ADR-008, gate 4). In the struct's filler: the size stays 108 bytes, so no unit's
    // Wi-Fi resets; a record from before reads it as it was stored there, 0. Not a setting: only POST /api/v1/setup/done
    // sets it, and a factory reset clears it
    bool setup_done;
} eeprom_wireless_metadata_t;





#ifdef __cplusplus
extern "C" {
#endif


void wireless_task(void *);
bool wireless_init(void);
bool wireless_config_save();
// How long the station tries to join before it gives up (settings/wifi timeout_ms): an update's trial waits this long
uint32_t wireless_join_timeout_ms(void);

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

/*
    The set-up's networks and join (ADR-008, gate 4). Every join of the station, the start's, the minute's retry and
    the set-up's, is the station's state: idle, joining, joined, failed and why (wrong_key, timeout, failed). A join
    asked over the API is the set-up's: only while the unit is its own hotspot, which it keeps up meanwhile; joined,
    the network is stored and in effect at once, refused, nothing is stored.
*/
typedef struct {
    const char *state;      // "idle", "joining", "joined", "failed"
    char ssid[33];          // the network joined or tried; "" with none
    char address[24];       // the station's address on it; "" with none
    const char *reason;     // failed: "wrong_key", "timeout", "failed"; NULL otherwise
} wireless_station_info_t;
void wireless_get_station(wireless_station_info_t *station);

// A network heard by the scans, merged by name: its strongest signal, and its security as the settings name it
typedef struct {
    char ssid[33];
    int16_t rssi;           // dBm
    const char *auth;       // "open", "wpa_tkip", "wpa2_aes", "wpa2_mixed"
} wireless_network_t;
// A scan asked: two of them, merged; false while a join or a scan runs
bool wireless_scan_start(void);
bool wireless_scanning(void);
// The idx-th network heard, strongest first once the scan ends (as heard while it runs); false past the last
bool wireless_network_at(size_t idx, wireless_network_t *out);

typedef enum {
    WIRELESS_JOIN_TAKEN,        // the Wi-Fi task joins it now
    WIRELESS_JOIN_BUSY,         // a join or a scan runs
    WIRELESS_JOIN_NOT_HOTSPOT,  // the unit is on its network: a join would leave it
    WIRELESS_JOIN_BAD_SSID,     // 1 to 31 bytes
    WIRELESS_JOIN_BAD_KEY,      // 8 to 63 bytes for a network with security
    WIRELESS_JOIN_BAD_AUTH,     // not one of the settings' choices
} wireless_join_result_t;
// A join asked by `by` (the API's, the console's): its network, key and security (the settings' words)
wireless_join_result_t wireless_join(const char *ssid, const char *key, const char *auth, activity_by_t by);
// The console's `wifi join`: the stored network, with its stored key, through the same join
wireless_join_result_t wireless_join_stored(activity_by_t by);

// Set up: the set-up finished or skipped, or a network named (a unit set up before the set-up existed)
bool wireless_set_up(void);
// The set-up done, by whom: remembered and stored. Joined, the hotspot stops a moment later; hotspot_stays says
// whether it stays (not joined: the unit's only network). NULL when done, else why not
const char *wireless_setup_done(activity_by_t by, bool *hotspot_stays);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_H_