#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <u8g2.h>


#ifdef __cplusplus
extern "C" {
#endif

u8g2_t *get_display_handler(void);

void display_init(void);

#ifdef __cplusplus
}
#endif


#endif  // DISPLAY_H_