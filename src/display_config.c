#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "display_config.h"
#include "eeprom.h"
#include "common.h"

// Global config
static display_config_t display_config;

bool display_config_save(void) {
    bool is_ok = eeprom_write(EEPROM_DISPLAY_CONFIG_BASE_ADDR, (uint8_t *)&display_config, sizeof(display_config));
    return is_ok;
}

// Get the compiled display type (cannot be changed at runtime)
static display_type_t get_compiled_display_type(void) {
#if defined(USE_TFT35)
    return DISPLAY_TYPE_TFT35;
#elif defined(USE_TFT43)
    return DISPLAY_TYPE_TFT43;
#else
    return DISPLAY_TYPE_MINI_12864;
#endif
}

bool display_config_init(void) {
    bool is_ok;

    // Read configuration from EEPROM
    memset(&display_config, 0x0, sizeof(display_config));
    is_ok = eeprom_read(EEPROM_DISPLAY_CONFIG_BASE_ADDR, (uint8_t *)&display_config, sizeof(display_config));

    bool need_defaults = false;
    if (!is_ok) {
        printf("Unable to read display config from EEPROM at address %x, using defaults\n", EEPROM_DISPLAY_CONFIG_BASE_ADDR);
        need_defaults = true;
    } else if (display_config.data_rev != EEPROM_DISPLAY_CONFIG_DATA_REV) {
        need_defaults = true;
    }

    if (need_defaults) {
        display_config.data_rev = EEPROM_DISPLAY_CONFIG_DATA_REV;

        // Set defaults
        display_config.rotation = DISPLAY_ROTATION_0;
        display_config.brightness = 255;  // Max brightness
        display_config.inverted_encoder = false;

        // Write back (may fail if no EEPROM)
        if (is_ok && !display_config_save()) {
            printf("Unable to write display config to %x\n", EEPROM_DISPLAY_CONFIG_BASE_ADDR);
        }
    }

    // Always use compiled display type (firmware-specific)
    display_config.display_type = get_compiled_display_type();

    printf("Display config loaded: type=%d, rotation=%d, brightness=%d\n",
           display_config.display_type, display_config.rotation, display_config.brightness);
    return true;
}

display_type_t display_config_get_type(void) {
    return display_config.display_type;
}

void display_config_set_type(display_type_t type) {
    display_config.display_type = type;
}

display_config_t* display_config_get(void) {
    return &display_config;
}

bool http_rest_display_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings:
    // d0 (int): display_type (0=Mini12864, 1=TFT35, 2=TFT43) - READ ONLY, set by firmware
    // d1 (int): rotation (0=0°, 1=90°, 2=180°, 3=270°)
    // d2 (int): brightness (0-255)
    // d3 (bool): inverted_encoder
    // ee (bool): save to eeprom
    static char buf[256];
    bool save_to_eeprom = false;

    // Process parameters
    for (int idx = 0; idx < num_params; idx++) {
        // d0 (display_type) is ignored - it's firmware-specific and cannot be changed
        if (strcmp(params[idx], "d1") == 0) {
            int rot = atoi(values[idx]);
            if (rot >= DISPLAY_ROTATION_0 && rot <= DISPLAY_ROTATION_270) {
                display_config.rotation = (display_rotation_t)rot;
            }
        }
        else if (strcmp(params[idx], "d2") == 0) {
            int bright = atoi(values[idx]);
            if (bright >= 0 && bright <= 255) {
                display_config.brightness = (uint8_t)bright;
            }
        }
        else if (strcmp(params[idx], "d3") == 0) {
            display_config.inverted_encoder = string_to_boolean(values[idx]);
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Save to EEPROM if requested
    if (save_to_eeprom) {
        display_config_save();
    }

    // Response
    snprintf(buf, sizeof(buf),
             "%s"
             "{\"d0\":%d,\"d1\":%d,\"d2\":%d,\"d3\":%s}",
             http_json_header,
             display_config.display_type,
             display_config.rotation,
             display_config.brightness,
             boolean_to_string(display_config.inverted_encoder));

    size_t response_len = strlen(buf);
    file->data = buf;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
