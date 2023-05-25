#ifndef ACCESS_POINT_MODE_H_
#define ACCESS_POINT_MODE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint8_t access_point_mode_menu();

bool access_point_mode_start();
bool access_point_mode_stop();

#ifdef __cplusplus
}
#endif

#endif  // ACCESS_POINT_MODE_H_