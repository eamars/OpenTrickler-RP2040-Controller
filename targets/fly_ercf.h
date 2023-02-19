/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FLY_ERCF_CONFIG_H_
#define FLY_ERCF_CONFIG_H_

/** \file pico.h
 *  \defgroup pico_base pico_base
 *
 * Core types and macros for the Raspberry Pi Pico SDK. This header is intended to be included by all source code
 * as it includes configuration headers and overrides in the correct order
 *
 * This header may be included by assembly code
*/

#define	__PICO_STRING(x)	#x
#define	__PICO_XSTRING(x)	__PICO_STRING(x)

#include "pico/types.h"
#include "pico/version.h"

// PICO_CONFIG: PICO_CONFIG_HEADER, unquoted path to header include in place of the default pico/config.h which may be desirable for build systems which can't easily generate the config_autogen header, group=pico_base
#ifdef PICO_CONFIG_HEADER
#include __PICO_XSTRING(PICO_CONFIG_HEADER)
#else
#include "pico/config.h"
#endif
#include "pico/platform.h"
#include "pico/error.h"

// Board specific settings
#define DISPlAY0_SPI spi1
#define DISPLAY0_CS_PIN 13
#define DISPLAY0_RX_PIN 12  // miso
#define DISPLAY0_TX_PIN 11  // mosi
#define DISPLAY0_SCK_PIN 10
#define DISPLAY0_A0_PIN 24
#define DISPLAY0_RESET_PIN 25

#define BUTTON0_ENCODER_PIN1 27
#define BUTTON0_ENCODER_PIN2 28
#define BUTTON0_RST_PIN 29
#define NEOPIXEL_PIN 26

#define COARSE_MOTOR_UART uart0
#define COARSE_MOTOR_UART_RX_PIN 1
#define COARSE_MOTOR_UART_TX_PIN 0

#define FINE_MOTOR_UART uart1
#define FINE_MOTOR_UART_RX_PIN 9
#define FINE_MOTOR_UART_TX_PIN 8

#endif  // FLY_ERCF_CONFIG_H_
