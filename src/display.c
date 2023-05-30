#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <u8g2.h>


#include "display.h"
#include "http_rest.h"


// Local variables
u8g2_t display_handler;


u8g2_t * get_display_handler(void) {
    return &display_handler;
}


bool http_get_display_buffer(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t buffer_size = 8 * u8g2_GetBufferTileHeight(&display_handler) * u8g2_GetBufferTileWidth(&display_handler);
    file->data = u8g2_GetBufferPtr(&display_handler);
    file->len = buffer_size;
    file->index = buffer_size;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}