#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "configuration.h"
#include "eeprom.h"

// Include only for PICO board with specific flash chip
#include "pico/unique_id.h"


SemaphoreHandle_t eeprom_access_mutex = NULL;


void cat24c256_eeprom_init() {
    // Initialize I2C bus with 400k baud rate
    i2c_init(EEPROM_I2C, 400 * 1000);

    // Initialize PINs as I2C function
    gpio_set_function(EEPROM_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(EEPROM_SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(EEPROM_SDA_PIN);
    gpio_pull_up(EEPROM_SCL_PIN);

    // Make the I2C pins available to picotool
    // bi_decl(bi_2pins_with_func(EEPROM_SDA_PIN, EEPROM_SCL_PIN, GPIO_FUNC_I2C));
}


bool cat24c256_write(uint16_t data_addr, uint8_t * data, size_t len) {
    uint8_t buf[len + 2];  // Include first two bytes for address
    buf[0] = (data_addr >> 8) & 0xFF; // High byte of address
    buf[1] = data_addr & 0xFF; // Low byte of address

    // Copy data to buffer
    memcpy(&buf[2], data, len);

    // Send to the EEPROM
    int ret;
    ret = i2c_write_blocking(EEPROM_I2C, EEPROM_ADDR, buf, len + 2, false);
    return ret != PICO_ERROR_GENERIC;
}


bool cat24c256_read(uint16_t data_addr, uint8_t * data, size_t len) {
    uint8_t buf[2];  // Include first two bytes for address
    buf[0] = (data_addr >> 8) & 0xFF; // High byte of address
    buf[1] = data_addr & 0xFF; // Low byte of address

    i2c_write_blocking(EEPROM_I2C, EEPROM_ADDR, buf, 2, true);

    int bytes_read;
    bytes_read = i2c_read_blocking(EEPROM_I2C, EEPROM_ADDR, data, len, false);

    return bytes_read == len;
}


bool eeprom_init(void) {
    bool is_ok = true;
    eeprom_access_mutex = xSemaphoreCreateMutex();

    if (eeprom_access_mutex == NULL) {
        printf("Unable to create EEPROM mutex\n");
        return false;
    }
    
    cat24c256_eeprom_init();

    // Read data revision, if match then move forward
    eeprom_metadata_t metadata;
    is_ok = eeprom_read(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_METADATA_BASE_ADDR);
        return false;
    }

    if (metadata.eeprom_metadata_rev != EEPROM_METADATA_REV) {
        // Do some data migration or erase the data
        printf("EEPROM data revision: %x, Firmware EEPROM data revision: %x, Requires migration\n", metadata.eeprom_metadata_rev, EEPROM_METADATA_REV);

        // Update some data
        metadata.eeprom_metadata_rev = EEPROM_METADATA_REV;

        // Write data back
        is_ok = eeprom_write(EEPROM_METADATA_BASE_ADDR, (uint8_t *) &metadata, sizeof(eeprom_metadata_t));
        if (!is_ok) {
             printf("Unable to write to %x\n", EEPROM_METADATA_BASE_ADDR);
            return false;
        }
    }

    return is_ok;
}


static void _take_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreTake(eeprom_access_mutex, portMAX_DELAY);
    }
}

static void _give_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreGive(eeprom_access_mutex);
    }
}


bool eeprom_read(uint16_t data_addr, uint8_t * data, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();
    bool is_ok;

    _take_mutex(scheduler_state);

    is_ok = cat24c256_read(data_addr, data, len);

    _give_mutex(scheduler_state);

    return is_ok;
}


bool eeprom_write(uint16_t data_addr, uint8_t * data, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();
    bool is_ok;

    _take_mutex(scheduler_state);

    is_ok = cat24c256_write(data_addr, data, len);

    _give_mutex(scheduler_state);

    return is_ok;
}


