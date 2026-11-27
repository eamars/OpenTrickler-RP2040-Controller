/*
 * The system on the RP chips (chip.h): the Pico SDK's timer, watchdog, unique id, random numbers and stdio.
 */
#include "chip.h"

#include <string.h>

#include "hardware/watchdog.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

_Static_assert(CHIP_ID_BYTES == PICO_UNIQUE_BOARD_ID_SIZE_BYTES, "the chip's id is the flash's unique id");


uint64_t chip_time_us(void) {
    return time_us_64();
}


void chip_busy_wait_us(uint32_t us) {
    busy_wait_us(us);
}


void chip_restart(uint32_t after_ms) {
    watchdog_reboot(0, 0, after_ms);
}


void chip_id(uint8_t id[CHIP_ID_BYTES]) {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    memcpy(id, board_id.id, CHIP_ID_BYTES);
}


uint32_t chip_random32(void) {
    return get_rand_32();
}


void chip_console_start(void) {
    stdio_init_all();
}


int chip_console_read(uint32_t timeout_us) {
    int c = getchar_timeout_us(timeout_us);
    return c == PICO_ERROR_TIMEOUT ? CHIP_CONSOLE_NONE : c;
}
