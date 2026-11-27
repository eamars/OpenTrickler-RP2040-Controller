#ifndef CHIP_SERIAL_H_
#define CHIP_SERIAL_H_

/*
 * A serial port (ADR-006, gate 7), by its number in the board's pin map (configuration.h): the scale's and the motor
 * drivers' one-wire line. A platform interface; the RP chips' is the Pico SDK's UARTs (platform/rp2/chip_serial_rp2.c),
 * read from their FIFO as bytes come. A read never waits unless it is given a time to.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What a read gives when no byte came
#define CHIP_SERIAL_NONE (-1)

typedef enum {
    CHIP_SERIAL_PARITY_NONE = 0,
    CHIP_SERIAL_PARITY_EVEN,
    CHIP_SERIAL_PARITY_ODD,
} chip_serial_parity_t;

// The port on its two pins at `baud`, 8 data bits, 1 stop bit, no parity until chip_serial_format() says otherwise
void chip_serial_open(unsigned int port, unsigned int tx_pin, unsigned int rx_pin, uint32_t baud);
void chip_serial_format(unsigned int port, unsigned int data_bits, unsigned int stop_bits, chip_serial_parity_t parity);
void chip_serial_baud(unsigned int port, uint32_t baud);

// A byte received, or CHIP_SERIAL_NONE when none is there. It never waits
int chip_serial_read(unsigned int port);

/*
 * A byte received within `timeout_us`, or CHIP_SERIAL_NONE. A wait of a tick or more sleeps the task until a byte
 * comes, as ESP-IDF's uart_read_bytes() does (ADR-017, gate 2); a shorter one, or one before the scheduler starts,
 * spins. One task waits on a port at a time
 */
int chip_serial_read_within(unsigned int port, uint32_t timeout_us);

// Bytes sent: it returns once the port has taken them all, and they go out after
void chip_serial_write(unsigned int port, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // CHIP_SERIAL_H_
