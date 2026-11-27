#include <FreeRTOS.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "eeprom.h"


const char * true_string = "true";
const char * false_string = "false";
const char * boolean_to_string(bool var) {
    return var ? true_string : false_string;
}

bool string_to_boolean(char * s) {
    bool var = false;

    if (strcmp(s, true_string) == 0) {
        var = true;
    }

    return var;
}


int weight_to_string(char * output_decimal_str, size_t output_size, float var, decimal_places_t decimal_places) {
    if (output_size == 0) {
        return 0;
    }

    switch (decimal_places) {
        case DP_2:
            return snprintf(output_decimal_str, output_size, "%0.02f", var);
        case DP_3:
            return snprintf(output_decimal_str, output_size, "%0.03f", var);
        default:
            output_decimal_str[0] = '\0';
            return 0;
    }
}


int float_to_string(char * output_decimal_str, float var, decimal_places_t decimal_places) {
    int return_value = 0;
    
    switch (decimal_places) {
        case DP_2:
            return_value = sprintf(output_decimal_str, "%0.02f", var);
            break;
        case DP_3:
            return_value = sprintf(output_decimal_str, "%0.03f", var);
            break;
        default:
            break;
    }
    
    return return_value;
}


// POLYNOMIAL 0xEDB88320
const uint32_t crc32_table[256] = {
0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};
uint32_t software_crc32_continue(uint32_t crc, const void * data, size_t length) {
    const uint8_t *p = (const uint8_t *)data;
    crc ^= 0xffffffff;

    while (length--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xffffffff;
}


uint32_t software_crc32(void * data, size_t length) {
    return software_crc32_continue(0, data, length);
}


/*
    The CRC of what each module last stored, or read at the start (ADR-003, gate 3): `saved` is the struct in RAM
    against it, so a write from the menu or the console counts as one from the web does. One entry an address, with
    the record's size, so a part of it can be read back as stored (config_read_part).
*/
#define CONFIG_STORED_MAX 16
static struct {
    uint16_t addr;
    uint16_t size;
    uint32_t crc;
} config_stored[CONFIG_STORED_MAX];
static size_t config_stored_count = 0;


static void config_stored_set(uint16_t addr, uint32_t crc, size_t size) {
    for (size_t idx = 0; idx < config_stored_count; idx += 1) {
        if (config_stored[idx].addr == addr) {
            config_stored[idx].crc = crc;
            config_stored[idx].size = (uint16_t) size;
            return;
        }
    }
    if (config_stored_count < CONFIG_STORED_MAX) {
        config_stored[config_stored_count].addr = addr;
        config_stored[config_stored_count].size = (uint16_t) size;
        config_stored[config_stored_count].crc = crc;
        config_stored_count += 1;
    }
}


// The writes a trial holds: the latest of each address
static bool config_holding = false;
static struct {
    uint16_t addr;
    uint16_t size;
    bool defaults;
    uint8_t *data;
} config_held[CONFIG_STORED_MAX];
static size_t config_held_count = 0;


void config_hold_writes(bool hold) {
    config_holding = hold;
}


static bool config_hold(uint16_t addr, const void * cfg, size_t size, bool defaults) {
    size_t idx = 0;
    while (idx < config_held_count && config_held[idx].addr != addr) {
        idx += 1;
    }
    if (idx == CONFIG_STORED_MAX) {
        printf("Config: no room to hold the write at 0x%04x\n", addr);
        return false;
    }
    if (idx == config_held_count) {
        config_held[idx].data = malloc(size);
        if (config_held[idx].data == NULL) {
            printf("Config: no memory to hold the write at 0x%04x\n", addr);
            return false;
        }
        config_held[idx].addr = addr;
        config_held[idx].size = (uint16_t) size;
        config_held[idx].defaults = false;
        config_held_count += 1;
    }
    memcpy(config_held[idx].data, cfg, size);
    config_held[idx].defaults = config_held[idx].defaults || defaults;
    config_stored_set(addr, software_crc32((void *) cfg, size), size);
    printf("Config: on trial, the write at 0x%04x held until the update is confirmed\n", addr);
    return true;
}


size_t config_write_held(uint16_t *reset, size_t room) {
    config_holding = false;
    size_t reset_count = 0;
    for (size_t idx = 0; idx < config_held_count; idx += 1) {
        if (!save_config_now(config_held[idx].addr, config_held[idx].data, config_held[idx].size)) {
            printf("Config: the held write at 0x%04x failed\n", config_held[idx].addr);
        }
        if (config_held[idx].defaults && reset_count < room) {
            reset[reset_count++] = config_held[idx].addr;
        }
        free(config_held[idx].data);
    }
    config_held_count = 0;
    return reset_count;
}


bool config_defaults_held(uint16_t addr) {
    for (size_t idx = 0; idx < config_held_count; idx += 1) {
        if (config_held[idx].addr == addr) {
            return config_held[idx].defaults;
        }
    }
    return false;
}


bool config_read_stored(uint16_t addr, void * cfg, size_t size) {
    uint32_t received_crc32;
    uint8_t *buf = malloc(size + sizeof(received_crc32));
    if (buf == NULL) {
        return false;
    }
    bool is_ok = eeprom_read(addr, buf, size + sizeof(received_crc32));
    uint16_t received_rev;
    memcpy(&received_rev, buf, sizeof(received_rev));
    memcpy(&received_crc32, buf + size, sizeof(received_crc32));
    is_ok = is_ok && received_rev == 0 && software_crc32(buf, size) == received_crc32;
    if (is_ok) {
        memcpy(cfg, buf, size);
    }
    free(buf);
    return is_ok;
}


bool config_read_part(uint16_t addr, void * out, size_t size) {
    for (size_t idx = 0; idx < config_stored_count; idx += 1) {
        uint16_t record = config_stored[idx].addr;
        size_t record_size = config_stored[idx].size;
        if (addr < record || addr + size > record + record_size) {
            continue;
        }
        size_t offset = addr - record;
        // On a trial, what is stored is the write held: the EEPROM may still hold an earlier version's record
        for (size_t held = 0; held < config_held_count; held += 1) {
            if (config_held[held].addr == record) {
                memcpy(out, config_held[held].data + offset, size);
                return true;
            }
        }
        uint8_t *whole = malloc(record_size);
        bool is_ok = whole != NULL && config_read_stored(record, whole, record_size);
        if (is_ok) {
            memcpy(out, whole + offset, size);
        }
        free(whole);
        return is_ok;
    }
    return false;
}


bool config_is_saved(uint16_t addr, const void * cfg, size_t size) {
    uint32_t crc = software_crc32((void *) cfg, size);
    for (size_t idx = 0; idx < config_stored_count; idx += 1) {
        if (config_stored[idx].addr == addr) {
            return config_stored[idx].crc == crc;
        }
    }
    return false;   // never read nor stored
}


/*
    A record's migration, one for every module (the owner, 2026-11-25: "Between new size and 1, we can try if CRC
    matches any try_size - 1 ... So if we only append new item to the list, then the non-corrupted data should always
    match"; 2026-11-26: append, never repurpose, ignore and discard what is not known). Fields are only appended: at the
    record's end, or for a record that is a header and an array (the powders, the motors), at each element's end, the
    header as it was. A record whose CRC32 fails at its size is looked for at each size it could have had, by an
    earlier version or a later one, within its slot; found, its part is filled into the struct, element by element,
    the rest at the defaults. A wrong match is a chance of one in about four billion for each size tried.
*/
static const config_layout_t config_plain = {.header = 0, .element = 0, .count = 0};


// The record's known earlier layout stored at `stored` bytes, or NULL
static const config_past_layout_t *config_past(const config_layout_t *layout, size_t stored) {
    for (size_t idx = 0; idx < layout->past_count; idx += 1) {
        if (layout->past[idx].size == stored) {
            return &layout->past[idx];
        }
    }
    return NULL;
}


/*
    Whether a record of this layout can have been stored at `stored` bytes: one of its known earlier layouts, or a
    size it has been append-only from (below it, a prefix would be read shifted: ADR-009's review, B1)
*/
static bool config_size_fits(const config_layout_t *layout, size_t stored) {
    if (config_past(layout, stored) != NULL) {
        return true;
    }
    if (stored < layout->append_only_from) {
        return false;
    }
    if (layout->element == 0) {
        return stored > sizeof(uint16_t);       // past the old revision
    }
    return stored > layout->header && (stored - layout->header) % layout->count == 0;
}


/*
    The size the record in the slot was stored at, other than `size`: the longest below it (an earlier version's),
    else the shortest above it (a later version's); 0 when none. The CRC is carried from one size to the next, so the
    slot is read through once
*/
static size_t config_stored_size(const uint8_t *slot, size_t room, size_t size, const config_layout_t *layout) {
    uint32_t crc = 0;
    size_t shorter = 0;
    for (size_t stored = 1; stored + sizeof(crc) <= room; stored += 1) {
        crc = software_crc32_continue(crc, slot + stored - 1, 1);
        if (stored > size && shorter > 0) {
            break;
        }
        if (stored == size || !config_size_fits(layout, stored)) {
            continue;
        }
        uint32_t held;
        memcpy(&held, slot + stored, sizeof(held));
        if (crc == held) {
            if (stored > size) {
                return stored;
            }
            shorter = stored;
        }
    }
    return shorter;
}


/*
    A record stored at `stored` bytes, into the struct: the defaults, then what it holds of each field: a known earlier
    layout field by field (a field's bytes then into its place now, a narrower one widened with zeros), else element by
    element as a prefix of each
*/
static void config_fill(void *cfg, const void *default_cfg, size_t size, const uint8_t *slot, size_t stored,
                        const config_layout_t *layout) {
    memcpy(cfg, default_cfg, size);
    const config_past_layout_t *past = config_past(layout, stored);
    if (past != NULL) {
        for (uint8_t idx = 0; idx < past->field_count; idx += 1) {
            const config_field_map_t *field = &past->fields[idx];
            uint8_t *place = (uint8_t *) cfg + field->now;
            memset(place, 0, field->now_size);
            memcpy(place, slot + field->was, field->was_size < field->now_size ? field->was_size : field->now_size);
        }
        return;
    }
    if (layout->element == 0) {
        memcpy(cfg, slot, stored < size ? stored : size);
        return;
    }
    memcpy(cfg, slot, layout->header);
    size_t was = (stored - layout->header) / layout->count;
    size_t part = was < layout->element ? was : layout->element;
    for (size_t idx = 0; idx < layout->count; idx += 1) {
        memcpy((uint8_t *) cfg + layout->header + idx * layout->element, slot + layout->header + idx * was, part);
    }
}


bool load_config(uint16_t addr, void * cfg, const void * default_cfg, size_t size) {
    return load_config_layout(addr, cfg, default_cfg, size, NULL);
}


bool load_config_layout(uint16_t addr, void * cfg, const void * default_cfg, size_t size,
                        const config_layout_t * layout) {
    bool is_ok;
    uint32_t calculated_crc32;

    if (layout == NULL) {
        layout = &config_plain;
    }
    else if (layout->element > 0 &&
             (layout->count == 0 || layout->header + layout->element * layout->count != size)) {
        printf("Config: the layout at 0x%04x is not its struct's: read as a plain record\n", addr);
        layout = &config_plain;
    }


    // Prepare rx buffer
    size_t read_size = size + sizeof(calculated_crc32);
    uint8_t * buf = malloc(read_size);

    if (!buf) {
        printf("Unable to allocate buffer with size: %u\n", (unsigned) read_size);
        return false;
    }

    // Read data
    is_ok = eeprom_read(addr, buf, read_size);

    // Unable to read from eeprom, then we will quit
    if (!is_ok) {
        printf("Unable to read from addr: 0x%04x\n", (unsigned) addr);
        free(buf);
        return is_ok;
    }

    // A record's first two bytes, its revision before v1.17, are 0 in every record since
    uint16_t received_rev;
    memcpy(&received_rev, buf, sizeof(received_rev));

    // Verify crc
    uint32_t received_crc32 = 0;

    calculated_crc32 = software_crc32(buf, size);
    memcpy(&received_crc32, buf + size, sizeof(received_crc32));

    // Another version's record, found at the size it was stored at: what it holds of each field kept, the rest at the
    // defaults, and stored at this version's size at once (a later version's fields discarded), held on trial as any
    // write is. Nothing is reset
    if (received_rev == 0 && calculated_crc32 != received_crc32 && size + sizeof(received_crc32) <= EEPROM_SLOT_SIZE) {
        uint8_t *slot = malloc(EEPROM_SLOT_SIZE);
        size_t stored = slot != NULL && eeprom_read(addr, slot, EEPROM_SLOT_SIZE) ?
                        config_stored_size(slot, EEPROM_SLOT_SIZE, size, layout) : 0;
        if (stored > 0) {
            printf("Config: the record at 0x%04x was stored %u bytes long by %s version: %s\n", addr, (unsigned) stored,
                   stored < size ? "an earlier" : "a later",
                   config_past(layout, stored) != NULL ? "read field by field from its layout then" :
                   stored < size ? "kept, the appended fields at their defaults" : "its fields read, the rest discarded");
            config_fill(cfg, default_cfg, size, slot, stored, layout);
            free(slot);
            free(buf);
            return save_config(addr, cfg, size);
        }
        free(slot);
    }

    // We will validate if the rev (first 2 byte) is 0, AND CRC check matches. 
    if ((received_rev != 0) || (calculated_crc32 != received_crc32)) {
        if (received_rev != 0) {
            printf("EEPROM is unlikely initialized, will populate with default configuration\n");
        }
        if (calculated_crc32 != received_crc32) {
            printf("CRC32 mismatch at address %x, received: %08" PRIX32 ", calculated: %08" PRIX32 "\n", (unsigned) addr,
                   received_crc32, calculated_crc32);
        }
        // Apply the default configuration: on trial, held as the defaults, written only once the update is kept
        free(buf);
        memcpy(cfg, default_cfg, size);
        if (config_holding) {
            return config_hold(addr, cfg, size, true);
        }
        return save_config(addr, cfg, size);
    }
    else {
        printf("Configuration read successfully\n");

        // Copy from buffer to config
        memcpy(cfg, buf, size);
        free(buf);
        config_stored_set(addr, calculated_crc32, size);
    }

    return true;
}


bool save_config(uint16_t addr, void * cfg, size_t size) {
    if (config_holding) {
        return config_hold(addr, cfg, size, false);
    }
    return save_config_now(addr, cfg, size);
}


bool save_config_now(uint16_t addr, void * cfg, size_t size) {
    bool is_ok;
    uint32_t calculated_crc32;

    // Copy data into buffer then append with crc32
    size_t write_size = size + sizeof(calculated_crc32);
    uint8_t * buf = malloc(write_size);

    if (!buf) {
        printf("Unable to allocate buffer with size: %u\n", (unsigned) write_size);
        return false;
    }

    // Calculate CRC
    calculated_crc32 = software_crc32(cfg, size);

    // Build write buffer by copying configuration appended with crc32
    memcpy(buf, cfg, size);
    memcpy(buf + size, &calculated_crc32, sizeof(calculated_crc32));

    // Write to EEPROM
    is_ok = eeprom_write(addr, buf, write_size);
    if (!is_ok) {
        printf("Unable to write to addr: 0x%04x\n", addr);
    }
    else {
        printf("Configuration write successfully\n");
        config_stored_set(addr, calculated_crc32, size);
    }

    free(buf);
    return is_ok;
}