/*
 * The one-wire serial line of Trinamic's stepper drivers: see tmc_uart.h.
 */
#include <string.h>

#include "tmc_uart.h"

#define TMC_UART_SYNC 0x05
#define TMC_UART_MASTER_ADDR 0xFF   // Every answer is addressed to the master
#define TMC_UART_WRITE_FLAG 0x80

#define TMC_UART_READ_REQUEST_LEN 4
#define TMC_UART_FRAME_LEN 8        // A write, and the answer to a read


// CRC8, polynomial 0x07, each byte taken lowest bit first (the drivers' datasheets)
static uint8_t tmc_uart_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;

    for (size_t idx = 0; idx < len; idx += 1) {
        uint8_t current_byte = data[idx];

        for (int bit = 0; bit < 8; bit += 1) {
            if ((crc >> 7) ^ (current_byte & 0x01)) {
                crc = (crc << 1) ^ 0x07;
            }
            else {
                crc = crc << 1;
            }
            current_byte >>= 1;
        }
    }

    return crc;
}


static bool tmc_uart_read_once(tmc_uart_t *bus, uint8_t reg, uint32_t *value) {
    uint8_t request[TMC_UART_READ_REQUEST_LEN] = {TMC_UART_SYNC, bus->addr, reg, 0};
    uint8_t answer[TMC_UART_FRAME_LEN];

    request[3] = tmc_uart_crc8(request, sizeof(request) - 1);

    if (!bus->exchange(request, sizeof(request), answer, sizeof(answer))) {
        return false;
    }

    // The answer is the frame a write of this register and value would be, addressed to the master
    if (answer[0] != TMC_UART_SYNC || answer[1] != TMC_UART_MASTER_ADDR || answer[2] != reg ||
        answer[7] != tmc_uart_crc8(answer, sizeof(answer) - 1)) {
        return false;
    }

    *value = ((uint32_t) answer[3] << 24) | ((uint32_t) answer[4] << 16) | ((uint32_t) answer[5] << 8) | answer[6];
    return true;
}


void tmc_uart_init(tmc_uart_t *bus, tmc_uart_exchange_fn exchange, uint8_t addr) {
    memset(bus, 0x0, sizeof(tmc_uart_t));
    bus->exchange = exchange;
    bus->addr = addr;
}


bool tmc_uart_read(tmc_uart_t *bus, uint8_t reg, uint32_t *value) {
    for (int attempt = 0; attempt < TMC_UART_TRIES; attempt += 1) {
        if (tmc_uart_read_once(bus, reg, value)) {
            return true;
        }
        bus->read_retries += 1;
    }

    bus->failures += 1;
    return false;
}


bool tmc_uart_write(tmc_uart_t *bus, uint8_t reg, uint32_t value) {
    uint8_t frame[TMC_UART_FRAME_LEN] = {
        TMC_UART_SYNC, bus->addr, reg | TMC_UART_WRITE_FLAG,
        (uint8_t) (value >> 24), (uint8_t) (value >> 16), (uint8_t) (value >> 8), (uint8_t) value,
        0,
    };
    uint32_t count;

    frame[7] = tmc_uart_crc8(frame, sizeof(frame) - 1);

    for (int attempt = 0; attempt < TMC_UART_TRIES; attempt += 1) {
        if (!bus->ifcnt_known) {
            if (!tmc_uart_read(bus, TMC_UART_REG_IFCNT, &count)) {
                return false;
            }
            bus->ifcnt = (uint8_t) count;
            bus->ifcnt_known = true;
        }
        uint8_t count_before = bus->ifcnt;

        // Whether the frame went out whole is not asked here: the driver's count says whether it arrived
        bus->exchange(frame, sizeof(frame), NULL, 0);

        if (!tmc_uart_read(bus, TMC_UART_REG_IFCNT, &count)) {
            bus->ifcnt_known = false;
            return false;
        }
        bus->ifcnt = (uint8_t) count;

        if (bus->ifcnt == (uint8_t) (count_before + 1)) {
            bus->writes += 1;
            return true;
        }
        bus->write_retries += 1;
    }

    bus->failures += 1;
    return false;
}
