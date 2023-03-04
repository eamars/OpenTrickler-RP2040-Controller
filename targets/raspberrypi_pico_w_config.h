#ifndef RASPBERRYPI_PICO_W_CONFIG_H_
#define RASPBERRYPI_PICO_W_CONFIG_H_

#include "pico/cyw43_arch.h"


#define LED_SET(pin, state) cyw43_arch_gpio_put(pin, state);


// Settings
#define WATCHDOG_LED_PIN CYW43_WL_GPIO_LED_PIN

#define DISPlAY0_SPI spi0
#define DISPLAY0_RX_PIN 16
#define DISPLAY0_TX_PIN 19
#define DISPLAY0_CS_PIN 17
#define DISPLAY0_SCK_PIN 18
#define DISPLAY0_A0_PIN 20
#define DISPLAY0_RESET_PIN 21

#define BUTTON0_ENCODER_PIN1 15
#define BUTTON0_ENCODER_PIN2 14
#define BUTTON0_ENC_PIN 22
#define BUTTON0_RST_PIN 12
#define NEOPIXEL_PIN 26

#define MOTOR_UART uart1
#define MOTOR_UART_TX 4
#define MOTOR_UART_RX 5

#define COARSE_MOTOR_ADDR 0
#define FINE_MOTOR_ADDR 1

#define SCALE_UART uart0
#define SCALE_UART_TX 0
#define SCALE_UART_RX 1


#endif  // RASPBERRYPI_PICO_W_CONFIG_H_