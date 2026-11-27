#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "configuration.h"
#include "cat24c256_eeprom.h"

#include "chip.h"
#include "chip_i2c.h"


#define PAGE_SIZE   64  // 64 byte page size


void cat24c256_eeprom_init() {
    // The I2C bus at 400 kHz, with the chip's pull-ups
    chip_i2c_open(EEPROM_I2C, EEPROM_SDA_PIN, EEPROM_SCL_PIN, 400 * 1000, true);

    // Make the I2C pins available to picotool
    // bi_decl(bi_2pins_with_func(EEPROM_SDA_PIN, EEPROM_SCL_PIN, GPIO_FUNC_I2C));
}


bool _cat24c256_write_page(uint16_t data_addr, uint8_t * data, size_t len){
    uint8_t buf[len + 2];  // Include first two bytes for address
    buf[0] = (data_addr >> 8) & 0xFF; // High byte of address
    buf[1] = data_addr & 0xFF; // Low byte of address

    // Copy data to buffer
    memcpy(&buf[2], data, len);

    // Send to the EEPROM
    return chip_i2c_write(EEPROM_I2C, EEPROM_ADDR, buf, len + 2);
}


bool cat24c256_write(uint16_t base_addr, uint8_t * data, size_t len) {
    uint16_t num_pages = len / PAGE_SIZE;
    bool is_ok;

    for (uint16_t page = 0; page <= num_pages; page += 1) {
        uint16_t offset = page * PAGE_SIZE;
        size_t write_size = PAGE_SIZE;
        if (page == num_pages) {
            write_size = len % PAGE_SIZE;
        }

        is_ok = _cat24c256_write_page(base_addr + offset, data + offset, write_size);
        chip_busy_wait_us(5 * 1000u);
        if (!is_ok) {
            return false;
        }
    }

    return true;
}


bool cat24c256_read(uint16_t data_addr, uint8_t * data, size_t len) {
    uint8_t buf[2];  // Include first two bytes for address
    buf[0] = (data_addr >> 8) & 0xFF; // High byte of address
    buf[1] = data_addr & 0xFF; // Low byte of address

    return chip_i2c_write_read(EEPROM_I2C, EEPROM_ADDR, buf, sizeof(buf), data, len);
}


bool cat24c256_eeprom_erase() {
    uint8_t dummy_buffer[PAGE_SIZE];
    memset(dummy_buffer, 0xff, PAGE_SIZE);

    
    for (size_t page=0; page < 512; page++) {
        size_t page_offset = page * PAGE_SIZE;
        cat24c256_write(page_offset, dummy_buffer, PAGE_SIZE);
    }
    return true;
}
