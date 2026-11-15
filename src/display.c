#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <u8g2.h>
#include <FreeRTOS.h>
#include <semphr.h>

#include "display.h"


// Local variables
u8g2_t display_handler;
SemaphoreHandle_t display_buffer_access_mutex = NULL;

u8g2_t * get_display_handler(void) {
    return &display_handler;
}

void acquire_display_buffer_access() {
    if (!display_buffer_access_mutex) {
        display_buffer_access_mutex = xSemaphoreCreateMutex();
    }

    assert(display_buffer_access_mutex);

    xSemaphoreTake(display_buffer_access_mutex, portMAX_DELAY);
}

void release_display_buffer_access() {
    assert(display_buffer_access_mutex);

    xSemaphoreGive(display_buffer_access_mutex);
}


/* u8g2 buffer structure can be decoded according to the description here: 
    https://github.com/olikraus/u8g2/wiki/u8g2reference#memory-structure-for-controller-with-u8x8-support

    Here is the Python script helping to explain how u8g2 buffer are arranged.

        with open(f, 'rb') as fp:
            display_buffer = fp.read()

        tile_width = 0x10  # 16 tiles per tile row

        for tile_row_idx in range(8):
            for bit in range(8):
                # Each tile row includes 16 * 8 bytes
                for byte_idx in range(tile_width * 8):
                    data_offset = byte_idx + tile_row_idx * tile_width * 8
                    data = display_buffer[data_offset]
                    if (1 << bit) & data:
                        print(' * ', end='')
                    else:
                        print('   ', end='')

                print()

*/
