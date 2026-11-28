#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"



typedef enum : uint8_t {
    AUTH_OPEN = 0,
    AUTH_WPA_TKIP_PSK = 1,
    AUTH_WPA2_AES_PSK = 2,
    AUTH_WPA2_MIXED_PSK = 3,
} wireless_auth_t;


typedef struct {
    uint16_t wireless_data_rev;
    char ssid[32];
    char pw[64];
    wireless_auth_t auth;
    uint32_t timeout_ms;
    bool enable;
    // The set-up finished or skipped (ADR-008, gate 4). In the struct's filler: the size stays 108 bytes, so no unit's
    // Wi-Fi resets; a record from before reads it as it was stored there, 0. Not a setting: only POST /api/v1/setup/done
    // sets it, and a factory reset clears it
    bool setup_done;
    // The web's PIN asked for (ADR-016, gate 3): 0 off, 1 on. A boolean in a word, so it starts past the 108 bytes the
    // release stored (a byte would lie in their tail padding); a record from before reads it as off
    uint32_t pin_required;
} eeprom_wireless_metadata_t;





/*
 * What follows the network (ADR-006, gate 6): handed over by the start-up's wiring, so the Wi-Fi names neither the
 * web nor the update. Each is called in the Wi-Fi's task but `reachable` at a server's first listen, which is in
 * that server's. Any may be NULL.
 */
typedef struct {
    // Once, when the unit's network is up, as a station or as its own access point: start what serves on it
    void (*network_up)(void);
    // The unit can be reached: a server listens (wireless_server_listening()), or the station joined later. True
    // when it is on the network it is set up for; false for a unit set up for Wi-Fi that fell back to its hotspot
    void (*reachable)(bool on_its_network);
    // The station is on its network: at the start, after a rejoin, at the set-up's end. The internet may be there
    void (*station_joined)(void);
} wireless_listener_t;

#ifdef __cplusplus
extern "C" {
#endif

// The start-up's wiring, before wireless_init(). The struct is kept by its address
void wireless_set_listener(const wireless_listener_t *listener);

// The unit's own names, for a server on its network: whether a request's Host is the unit's own (its address, or its
// host name with or without ".local", any port), and the unit's address, for a redirect
bool wireless_host_is_own(const char *host);
bool wireless_own_address(char *buf, size_t len);
// The unit's name on the network: "opentrickler-4A82"
const char *wireless_host_name(void);
// A server on the network says it listens, once: the listener's `reachable`, and its `station_joined` as a station
void wireless_server_listening(void);


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
    bool no_radio;          // the radio did not start (ADR-017, gate 1): mode is "none" for good
} wireless_info_t;
void wireless_get_info(wireless_info_t *info);
// The same without the signal, which asks the radio under lwIP's lock: what the screen reads at every refresh
void wireless_get_link(wireless_info_t *info);

// The radio did not start: the unit runs without the network (ADR-017, gate 1). False before its start
bool wireless_radio_failed(void);

// The station leaves its network, as when its router goes away: the console's `wifi leave` (ADR-004)
bool wireless_station_leave(void);

/*
    The set-up's networks and join (ADR-008, gate 4). Every join of the station, the start's, the minute's retry and
    the set-up's, is the station's state: idle, joining, joined, failed and why (wrong_password, timeout, failed). A join
    asked over the API is the set-up's: only while the unit is its own hotspot, which it keeps up meanwhile; joined,
    the network is stored and in effect at once, refused, nothing is stored.
*/
typedef struct {
    const char *state;      // "idle", "joining", "joined", "failed"
    char ssid[33];          // the network joined or tried; "" with none
    char address[24];       // the station's address on it; "" with none
    const char *reason;     // failed: "wrong_password", "timeout", "failed"; NULL otherwise
    uint32_t joining_ms;    // joining: since it began, so both screens count from the same start
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
// The idx-th network heard, strongest first; false past the last, and while a scan runs
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

/*
 * The web's PIN (ADR-016, gate 3): a check that whoever changes the unit over the network can read its screen. With
 * the switch on (settings/wifi pin_required, off by default), every write over the network needs the PIN, 4 digits
 * made at each start and shown on the Wi-Fi page; a client that reached the unit by its own hotspot needs none.
 * wireless_pin(): the PIN, its 4 digits. wireless_reached_by_hotspot(): the unit's address a client reached, as its 4
 * bytes in order, is the hotspot's while the hotspot is up.
 */
#define WIRELESS_PIN_DIGITS 4
bool wireless_pin_required(void);
const char *wireless_pin(void);
bool wireless_pin_matches(const char *digits);
bool wireless_reached_by_hotspot(const uint8_t reached[4]);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_H_