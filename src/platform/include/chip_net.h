#ifndef CHIP_NET_H_
#define CHIP_NET_H_

/*
 * The network's radio and what lies beside lwIP's sockets (ADR-006, gate 7): the Wi-Fi as a station and as the unit's
 * own hotspot, the hotspot's DHCP and DNS servers, a scan, and the unit's name announced by mDNS. A platform interface;
 * the RP chips' is the CYW43's on the Pico SDK, with lwIP's raw API under its lock (platform/rp2/chip_net_rp2.c). The
 * web and the update's fetch use lwIP's sockets as they are, the same on both chips. Called from the Wi-Fi's task, but
 * where said.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A network's security, as the unit joins it
typedef enum {
    CHIP_NET_OPEN = 0,
    CHIP_NET_WPA_TKIP,
    CHIP_NET_WPA2_AES,
    CHIP_NET_WPA2_MIXED,
} chip_net_security_t;

// How a join ended
typedef enum {
    CHIP_NET_JOINED = 0,
    CHIP_NET_WRONG_KEY,
    CHIP_NET_NOT_FOUND,         // no answer within its time
    CHIP_NET_JOIN_FAILED,
} chip_net_join_t;

// The radio started. False when it could not be: the unit has no network
bool chip_net_start(void);

// The station's interface up, beside the hotspot if it is up, the unit's name the one DHCP tells. Again: nothing
void chip_net_station_up(const char *host_name);

/*
 * The station joins a network, waiting at most `timeout_ms`: `key` NULL for an open one, `bssid` the access point to
 * take, NULL for any of the name's
 */
chip_net_join_t chip_net_join(const char *ssid, const char *key, chip_net_security_t security, const uint8_t *bssid,
                              uint32_t timeout_ms);

// The station leaves its network, as when its router goes away. False when it could not be asked
bool chip_net_leave(void);

// Whether the station's link is up. From any task
bool chip_net_joined(void);

// The station's address, and the hotspot's, as text; false when it has none. From any task
bool chip_net_station_address(char *buf, size_t len);
bool chip_net_hotspot_address(char *buf, size_t len);

// The station's signal, in dBm; false when it could not be read
bool chip_net_rssi(int32_t *rssi);

// An access point heard by a scan
typedef struct {
    const uint8_t *ssid;        // its network's name, not ended by a zero; 0 long when hidden
    uint8_t ssid_len;
    uint8_t bssid[6];
    int16_t rssi;
    uint8_t channel;
    chip_net_security_t security;
    bool joinable;              // false for a security the unit cannot join (WEP alone)
} chip_net_heard_t;

// Called for each access point heard, from the radio's own task
typedef void (*chip_net_heard_fn)(const chip_net_heard_t *heard);

// A scan begun, each access point given to `heard` as it is heard; false when it did not start
bool chip_net_scan_start(chip_net_heard_fn heard);
bool chip_net_scanning(void);

/*
 * The unit's own hotspot: its name, its key (WPA2), on `channel` (0: the radio's), at `address` with a /24 about it. Its
 * DHCP server gives the addresses; its DNS server answers every name with `address`, the set-up's captive portal. The
 * station stays as it is
 */
bool chip_net_hotspot_start(const char *name, const char *key, uint8_t channel, const char *address);
void chip_net_hotspot_stop(void);

// The addresses the hotspot's DHCP server has given since the start: a phone joined when it rises. From any task
uint32_t chip_net_hotspot_addresses_given(void);

// A service mDNS announces, over TCP
typedef struct {
    const char *instance;
    const char *type;           // "_http"
    uint16_t port;
} chip_net_service_t;

/*
 * The unit's name announced by mDNS on the station, "<host_name>.local", and its services, each with the same text
 * items. Once; what it is given stays its own (static)
 */
void chip_net_announce(const char *host_name, const chip_net_service_t *services, size_t service_count,
                       const char *const *txt, size_t txt_count);

// The radio's own LED (the Pico W's)
void chip_net_led(bool on);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_NET_H_
