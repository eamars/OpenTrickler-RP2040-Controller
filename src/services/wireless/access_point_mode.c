#include <stdbool.h>
#include <stdint.h>

#include "chip_net.h"

#include "access_point_mode.h"
#include "unit_id.h"
#include "wireless.h"


// The access point's key, the unit's own, made from its chip at the start (ADR-008, gate 4): the screen's Wi-Fi page
// shows it
static char access_point_key[UNIT_HOTSPOT_KEY_LENGTH + 1];


void access_point_mode_init(void) {
    unit_id_hotspot_key(access_point_key, sizeof(access_point_key));
}


const char *access_point_mode_key(void) {
    return access_point_key;
}


uint32_t access_point_mode_addresses_given(void) {
    return chip_net_hotspot_addresses_given();
}


// The hotspot, named as the unit, at ACCESS_POINT_MODE_ADDRESS, with its DHCP and DNS servers (chip_net.h)
bool access_point_mode_start(uint8_t channel) {
    return chip_net_hotspot_start(wireless_host_name(), access_point_mode_key(), channel, ACCESS_POINT_MODE_ADDRESS);
}


bool access_point_mode_stop(void) {
    chip_net_hotspot_stop();
    return true;
}
