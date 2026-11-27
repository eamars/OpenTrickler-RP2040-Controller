/*
 * A serial port on the RP chips (chip_serial.h): uart0 and uart1, read from their FIFO. A wait of a tick or more
 * sleeps on the port's receive interrupt, unmasked only while a task waits (ADR-017, gate 2).
 */
#include "chip_serial.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/time.h"

// The receive interrupts: the FIFO at its level, or a byte in it and the line quiet for 32 bits
#define CHIP_SERIAL_RECEIVED (UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS)

// Given by a port's receive interrupt to the task asleep on it
static SemaphoreHandle_t chip_serial_received[NUM_UARTS];


static uart_inst_t *chip_serial_instance(unsigned int port) {
    return port == 0 ? uart0 : uart1;
}


static void chip_serial_on_receive(uart_inst_t *uart) {
    // Masked again at once: the task it wakes reads the FIFO, and until it does the interrupt would stay up
    hw_clear_bits(&uart_get_hw(uart)->imsc, CHIP_SERIAL_RECEIVED);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(chip_serial_received[uart_get_index(uart)], &woken);
    portYIELD_FROM_ISR(woken);
}


static void chip_serial_on_receive_0(void) {
    chip_serial_on_receive(uart0);
}


static void chip_serial_on_receive_1(void) {
    chip_serial_on_receive(uart1);
}


void chip_serial_open(unsigned int port, unsigned int tx_pin, unsigned int rx_pin, uint32_t baud) {
    uart_inst_t *uart = chip_serial_instance(port);
    uart_init(uart, baud);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    // The receive interrupt, masked until a wait: without its semaphore a wait spins, as before
    unsigned int index = uart_get_index(uart);
    if (chip_serial_received[index] == NULL) {
        chip_serial_received[index] = xSemaphoreCreateBinary();
        if (chip_serial_received[index] != NULL) {
            irq_set_exclusive_handler(UART_IRQ_NUM(uart), index == 0 ? chip_serial_on_receive_0 : chip_serial_on_receive_1);
            irq_set_enabled(UART_IRQ_NUM(uart), true);
        }
    }
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
    if (uart_is_readable(uart)) {
        return (uint8_t) uart_getc(uart);
    }

    // Shorter than a tick, or with no scheduler to sleep in, the wait spins
    const uint32_t tick_us = 1000000u / configTICK_RATE_HZ;
    SemaphoreHandle_t received = chip_serial_received[uart_get_index(uart)];
    if (timeout_us < tick_us || received == NULL || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return uart_is_readable_within_us(uart, timeout_us) ? (uint8_t) uart_getc(uart) : CHIP_SERIAL_NONE;
    }

    // The task sleeps until a byte came or the time is up; a wake that finds neither (a give left from an earlier
    // wait, or a tick before the time) sleeps again for what is left
    uint64_t until_us = time_us_64() + timeout_us;
    while (true) {
        hw_set_bits(&uart_get_hw(uart)->imsc, CHIP_SERIAL_RECEIVED);
        uint64_t now_us = time_us_64();
        if (uart_is_readable(uart) || now_us >= until_us) {
            break;
        }
        xSemaphoreTake(received, (TickType_t) ((until_us - now_us + tick_us - 1) / tick_us));
    }
    hw_clear_bits(&uart_get_hw(uart)->imsc, CHIP_SERIAL_RECEIVED);
    return uart_is_readable(uart) ? (uint8_t) uart_getc(uart) : CHIP_SERIAL_NONE;
}


void chip_serial_write(unsigned int port, const uint8_t *data, size_t len) {
    uart_write_blocking(chip_serial_instance(port), data, len);
}
