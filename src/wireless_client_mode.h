#ifndef WIRELESS_CLIENT_MODE_H_
#define WIRELESS_CLIENT_MODE_H_

#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif


uint8_t wifi_scan();
const char * wifi_get_ssid_name(void *data, uint16_t index);
uint16_t wifi_get_ssid_count(void *data);

#ifdef __cplusplus
}
#endif


#endif  // WIRELESS_CLIENT_MODE_H_