#ifndef ACCESS_POINT_MODE_H_
#define ACCESS_POINT_MODE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint8_t access_point_mode_menu();

bool access_point_mode_start();
bool access_point_mode_stop();
// The access point's key made, from the chip, before the scheduler starts (ADR-008, gate 4)
void access_point_mode_init(void);
// The access point's key, as the screen's Wi-Fi page shows it (ADR-010, gate 4)
const char *access_point_mode_key(void);

#ifdef __cplusplus
}
#endif

#endif  // ACCESS_POINT_MODE_H_