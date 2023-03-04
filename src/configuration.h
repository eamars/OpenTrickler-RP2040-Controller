#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "hardware/spi.h"
#include "hardware/uart.h"

#ifdef RASPBERRYPI_PICO
#include "raspberrypi_pico_config.h"
#endif

#ifdef RASPBERRYPI_PICO_W
#include "raspberrypi_pico_w_config.h"
#endif


#endif  // CONFIGURATION_H_