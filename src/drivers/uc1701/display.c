/*
    The mini 12864's panel, a UC1701 on SPI (ADR-010, gate 4). Its set-up is the one u8g2's driver sent to it before
    (u8x8_d_uc1701_mini12864.c), byte for byte. The panel keeps the frame it shows in its own memory, in pages of 8
    rows, a byte a column with the top row the lowest bit, so only the pages that changed are sent.
*/
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "chip.h"
#include "chip_gpio.h"

#include "configuration.h"
#include "display.h"

#define DISPLAY_SPI_HZ (4000 * 1000)
// The controller has 132 columns; turned 180 degrees, the panel's 128 start at the fifth
#define DISPLAY_FLIPPED_COLUMN 4

static uint8_t display_first_column = 0;


static const uint8_t display_setup[] = {
    0xe2,           // reset
    0xae,           // display off
    0x40,           // start line 0
    0xa0,           // columns as wired
    0xc8,           // rows as wired
    0xa6,           // a 1 is a lit pixel, not inverted
    0xa2,           // bias 1/9
    0x2f,           // power: booster, regulator and follower on
    0xf8, 0x00,     // booster 4x
    0x23,           // V0 resistor ratio
    0x81, 0x27,     // contrast, set again below
    0xac,           // indicator
    0xae,           // display off
    0xa5,           // every pixel on, while off
};
static const uint8_t display_on[] = {
    0xa4,           // pixels from memory
    0xaf,           // display on
    0x81, 0x3f,     // contrast at its most, as before
};
static const uint8_t display_upright[] = {0xa0, 0xc8};
static const uint8_t display_flipped[] = {0xa1, 0xc0};


// Commands, with A0 low; the chip is selected around them
// A wait of the RTOS's when it runs, a busy one before it does (the panel is set up before the scheduler starts)
static void display_delay_ms(uint32_t ms, BaseType_t scheduler_state) {
    if (scheduler_state == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else {
        chip_busy_wait_us(ms * 1000u);
    }
}


static void display_commands(const uint8_t *commands, size_t len) {
    chip_gpio_put(DISPLAY0_CS_PIN, 0);
    chip_gpio_put(DISPLAY0_A0_PIN, 0);
    spi_write_blocking(DISPLAY0_SPI, commands, len);
    chip_gpio_put(DISPLAY0_CS_PIN, 1);
}


// One page of 128 columns at the panel's first column. spi_write_blocking returns once the last bit is out, so A0
// changes between bytes, never during one
static void display_write_page(int page, const uint8_t *columns) {
    uint8_t address[] = {
        (uint8_t) (0x10 | (display_first_column >> 4)),
        (uint8_t) (display_first_column & 0x0f),
        (uint8_t) (0xb0 | page),
    };
    chip_gpio_put(DISPLAY0_CS_PIN, 0);
    chip_gpio_put(DISPLAY0_A0_PIN, 0);
    spi_write_blocking(DISPLAY0_SPI, address, sizeof(address));
    chip_gpio_put(DISPLAY0_A0_PIN, 1);
    spi_write_blocking(DISPLAY0_SPI, columns, DISPLAY_WIDTH);
    chip_gpio_put(DISPLAY0_CS_PIN, 1);
}


void display_init(bool flipped) {
    spi_init(DISPLAY0_SPI, DISPLAY_SPI_HZ);
    gpio_set_function(DISPLAY0_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY0_TX_PIN, GPIO_FUNC_SPI);

    chip_gpio_output(DISPLAY0_CS_PIN, 1);
    chip_gpio_output(DISPLAY0_A0_PIN, 0);
    chip_gpio_output(DISPLAY0_RESET_PIN, 0);

    // The reset pulse, then 6 ms for the controller to come up
    BaseType_t scheduler_state = xTaskGetSchedulerState();
    chip_gpio_put(DISPLAY0_RESET_PIN, 1);
    display_delay_ms(1, scheduler_state);
    chip_gpio_put(DISPLAY0_RESET_PIN, 0);
    display_delay_ms(1, scheduler_state);
    chip_gpio_put(DISPLAY0_RESET_PIN, 1);
    display_delay_ms(6, scheduler_state);

    display_commands(display_setup, sizeof(display_setup));
    display_commands(display_on, sizeof(display_on));
    if (flipped) {
        display_commands(display_flipped, sizeof(display_flipped));
        display_first_column = DISPLAY_FLIPPED_COLUMN;
    }
    else {
        display_commands(display_upright, sizeof(display_upright));
        display_first_column = 0;
    }

    uint8_t blank[DISPLAY_WIDTH];
    memset(blank, 0, sizeof(blank));
    for (int page = 0; page < DISPLAY_PAGES; page += 1) {
        display_write_page(page, blank);
    }
}


void display_invert(bool inverted) {
    uint8_t command = inverted ? 0xa7 : 0xa6;
    display_commands(&command, 1);
}


void display_send_pages(const uint8_t *rows, uint8_t pages) {
    uint8_t columns[DISPLAY_WIDTH];
    for (int page = 0; page < DISPLAY_PAGES; page += 1) {
        if ((pages & (1u << page)) == 0) {
            continue;
        }
        // The page's 8 rows turned into columns. Most of a screen is blank, so a byte of 8 blank pixels is passed by
        memset(columns, 0, sizeof(columns));
        for (int bit = 0; bit < 8; bit += 1) {
            const uint8_t *row = rows + (page * 8 + bit) * DISPLAY_ROW_BYTES;
            uint8_t row_bit = (uint8_t) (1u << bit);
            for (int byte = 0; byte < DISPLAY_ROW_BYTES; byte += 1) {
                uint8_t pixels = row[byte];
                if (pixels == 0) {
                    continue;
                }
                uint8_t *column = columns + byte * 8;
                for (int x = 0; x < 8; x += 1) {
                    if (pixels & (0x80u >> x)) {
                        column[x] |= row_bit;
                    }
                }
            }
        }
        display_write_page(page, columns);
    }
}
