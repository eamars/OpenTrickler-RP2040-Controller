// The host's stand-in for the Pico SDK's pico/time.h (tests/host_test.py)
#ifndef HOST_PICO_TIME_H_
#define HOST_PICO_TIME_H_

#include <stdint.h>

static inline void busy_wait_us(uint64_t us) {
    (void) us;
}

#endif
