#ifndef TMC_UART_H_
#define TMC_UART_H_

/*
 * The one-wire serial line of Trinamic's stepper drivers (ADR-012, step 1): the TMC2209, and any other of their
 * chips that speaks the same frames. It knows frames, not chips, and neither pins nor the RTOS: the platform hands it
 * one function that puts bytes on the wire and brings the answer back.
 *
 * The rules are Klipper's (klippy/extras/tmc_uart.py, Kevin O'Connor, GPLv3):
 *   - a read is tried up to five times, and its answer counts only if it is a whole, well-formed frame for the
 *     register that was asked;
 *   - a write is checked. The driver counts the writes it accepts in IFCNT, eight bits that wrap. The count is read
 *     after each write and must be one more, modulo 256; if not, the write is tried again, up to five times.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The count of accepted writes: the same address on every chip that speaks these frames
#define TMC_UART_REG_IFCNT 0x02

// How often a read or a write is tried before it is given up
#define TMC_UART_TRIES 5

/*
 * One exchange on the wire: send tx_len bytes, then take the rx_len bytes the driver answers with (0 for a write).
 * False if the bytes were not seen on the wire as they were sent, or the answer did not come in time.
 */
typedef bool (*tmc_uart_exchange_fn)(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);

typedef struct {
    tmc_uart_exchange_fn exchange;
    uint8_t addr;               // The driver's address on the wire, set by its MS1 and MS2 pins

    bool ifcnt_known;
    uint8_t ifcnt;              // The driver's count of accepted writes, as last read

    // Counted since start-up, for the console
    uint32_t writes;            // Writes the driver was seen to take
    uint32_t write_retries;     // Tries of a write that the count did not confirm
    uint32_t read_retries;      // Tries of a read that brought no good answer
    uint32_t failures;          // Reads and writes given up
} tmc_uart_t;

void tmc_uart_init(tmc_uart_t *bus, tmc_uart_exchange_fn exchange, uint8_t addr);
bool tmc_uart_read(tmc_uart_t *bus, uint8_t reg, uint32_t *value);
bool tmc_uart_write(tmc_uart_t *bus, uint8_t reg, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif  // TMC_UART_H_
