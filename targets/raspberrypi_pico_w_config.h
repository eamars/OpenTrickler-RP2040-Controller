#ifndef RASPBERRYPI_PICO_W_CONFIG_H_
#define RASPBERRYPI_PICO_W_CONFIG_H_

#include "pico/cyw43_arch.h"

/* Specify board PIN mapping
    Reference: https://github.com/eamars/RaspberryPi-Pico-Motor-Expansion-Board?tab=readme-ov-file#peripherals
*/

#define WATCHDOG_LED_PIN CYW43_WL_GPIO_LED_PIN

#define DISPLAY0_SPI spi0
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
#define NEOPIXEL_PIN 13
#define NEOPIXEL_PWM3_PIN 28

#define MOTOR_UART uart1
#define MOTOR_UART_TX 4
#define MOTOR_UART_RX 5

#define COARSE_MOTOR_ADDR 0
#define COARSE_MOTOR_EN_PIN 6
#define COARSE_MOTOR_STEP_PIN 3 
#define COARSE_MOTOR_DIR_PIN 2

#define FINE_MOTOR_ADDR 1
#define FINE_MOTOR_EN_PIN 9
#define FINE_MOTOR_STEP_PIN 8
#define FINE_MOTOR_DIR_PIN 7

#define SCALE_UART uart0
#define SCALE_UART_BAUDRATE 19200
#define SCALE_UART_TX 0
#define SCALE_UART_RX 1

#define EEPROM_I2C i2c1
#define EEPROM_SDA_PIN 10
#define EEPROM_SCL_PIN 11
#define EEPROM_ADDR 0x50

#define SERVO0_PWM_PIN 26
#define SERVO1_PWM_PIN 27
#define SERVO_PWM_SLICE_NUM 5

#endif  // RASPBERRYPI_PICO_W_CONFIG_H_