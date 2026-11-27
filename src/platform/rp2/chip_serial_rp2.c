/*
 * A serial port on the RP chips (chip_serial.h): uart0 and uart1, read from their FIFO.
 */
#include "chip_serial.h"

#include "hardware/gpio.h"
#include "hardware/uart.h"


static uart_inst_t *chip_serial_instance(unsigned int port) {
    return port == 0 ? uart0 : uart1;
}


void chip_serial_open(unsigned int port, unsigned int tx_pin, unsigned int rx_pin, uint32_t baud) {
    uart_init(chip_serial_instance(port), baud);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
}


void chip_serial_format(unsigned int port, unsigned int data_bits, unsigned int stop_bits, chip_serial_parity_t parity) {
    uart_parity_t chip_parity = parity == CHIP_SERIAL_PARITY_EVEN ? UART_PARITY_EVEN :
                                (parity == CHIP_SERIAL_PARITY_ODD ? UART_PARITY_ODD : UART_PARITY_NONE);
    uart_set_format(chip_serial_instance(port), data_bits, stop_bits, chip_parity);
}


void chip_serial_baud(unsigned int port, uint32_t baud) {
    uart_set_baudrate(chip_serial_instance(port), baud);
}


int chip_serial_read(unsigned int port) {
    uart_inst_t *uart = chip_serial_instance(port);
    return uart_is_readable(uart) ? (uint8_t) uart_getc(uart) : CHIP_SERIAL_NONE;
}


int chip_serial_read_within(unsigned int port, uint32_t timeout_us) {
    uart_inst_t *uart = chip_serial_instance(port);
    return uart_is_readable_within_us(uart, timeout_us) ? (uint8_t) uart_getc(uart) : CHIP_SERIAL_NONE;
}


void chip_serial_write(unsigned int port, const uint8_t *data, size_t len) {
    uart_write_blocking(chip_serial_instance(port), data, len);
}
