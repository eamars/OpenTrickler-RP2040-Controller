#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <u8g2.h>

#ifdef __cplusplus
extern "C" {
#endif

u8g2_t *get_display_handler(void);
// The frame buffer's lock: the menu writes a whole frame under it, and the mirror and the snap copy one under it,
// so neither reads a frame half written (ADR-012's audit; ADR-010, gate 4)
void acquire_display_buffer_access(void);
void release_display_buffer_access(void);

// REST

#ifdef __cplusplus
}
#endif


#endif  // DISPLAY_H_