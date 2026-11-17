#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdbool.h>
#include <stdint.h>

// The mini 12864's panel (ADR-010, gate 4): 128 x 64 pixels, one bit a pixel, a UC1701 on SPI
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_ROW_BYTES (DISPLAY_WIDTH / 8)
#define DISPLAY_FRAME_BYTES (DISPLAY_ROW_BYTES * DISPLAY_HEIGHT)
// The panel's memory is in pages of 8 rows
#define DISPLAY_PAGES (DISPLAY_HEIGHT / 8)
#define DISPLAY_ALL_PAGES 0xffu

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set up the panel and clear it. Before the scheduler starts or after.
 *
 * @param flipped true to turn the picture 180 degrees, as a panel mounted upside down needs.
 */
void display_init(bool flipped);

/**
 * Send pages of a frame to the panel.
 *
 * @param rows the frame as rows: DISPLAY_HEIGHT rows of DISPLAY_ROW_BYTES, the leftmost pixel the top bit, 1 lit.
 * @param pages the pages to send, a bit each (page 0 is rows 0 to 7, the lowest bit).
 */
void display_send_pages(const uint8_t *rows, uint8_t pages);

/**
 * The whole panel shown inverted, or as it is: the panel's own command, so the frame is untouched. The mini 12864's
 * glass is negative: a lit pixel shows light on a dark ground, so inverted is dark on a lit glass.
 */
void display_invert(bool inverted);

#ifdef __cplusplus
}
#endif


#endif  // DISPLAY_H_
