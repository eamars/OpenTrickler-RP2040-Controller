#ifndef FIRMWARE_FOOTER_H_
#define FIRMWARE_FOOTER_H_

#include <stdint.h>


// On-flash OTA footer, appended to app.bin at build time by
// scripts/append_firmware_footer.py to produce app_ota.bin. Lives at a fixed
// address (the last FIRMWARE_FOOTER_SIZE bytes of the OTA staging region),
// regardless of the uploaded payload's actual size, so "is a valid image
// staged" is a stateless flash read that survives reboot.
#define FIRMWARE_FOOTER_MAGIC          0x4B52544FUL
#define FIRMWARE_FOOTER_FORMAT_VERSION 1
#define FIRMWARE_FOOTER_VERSION_STRLEN 32
#define FIRMWARE_FOOTER_HASH_STRLEN    16
#define FIRMWARE_FOOTER_RESERVED_LEN   60
#define FIRMWARE_FOOTER_SIZE           128

typedef enum {
    FIRMWARE_BOARD_TYPE_UNKNOWN = 0,
    FIRMWARE_BOARD_TYPE_PICO_W  = 1,
    FIRMWARE_BOARD_TYPE_PICO2_W = 2,
} firmware_board_type_t;

// reserved[0] is repurposed as an "install pending" flag: the live app sets
// it (and rewrites footer_crc32) when the user confirms Install, then
// reboots without touching the app region itself; the second-stage
// bootloader (bootloader/main.c) is what actually checks this flag, copies
// the staged image into the app region if set, clears it, and chain-loads
// into the app. This keeps the footer's size/layout stable across that
// change -- see the c-style skill's persisted-struct-compatibility rule,
// applied here to this on-flash struct the same way it applies to EEPROM.
#define FIRMWARE_FOOTER_INSTALL_PENDING_BYTE 0

typedef struct __attribute__((packed)) {
    uint32_t magic;                                          // FIRMWARE_FOOTER_MAGIC
    uint16_t footer_format_version;                          // FIRMWARE_FOOTER_FORMAT_VERSION
    uint16_t board_type;                                      // firmware_board_type_t
    uint32_t payload_size;                                    // bytes of app image before this footer
    uint32_t payload_crc32;                                   // over [0, payload_size)
    char     version_string[FIRMWARE_FOOTER_VERSION_STRLEN];  // NUL-padded
    char     vcs_hash[FIRMWARE_FOOTER_HASH_STRLEN];            // NUL-padded
    uint8_t  reserved[FIRMWARE_FOOTER_RESERVED_LEN];          // reserved[0] = install pending, rest zero-filled
    uint32_t footer_crc32;                                     // over all preceding footer bytes
} firmware_update_footer_t;

_Static_assert(sizeof(firmware_update_footer_t) == FIRMWARE_FOOTER_SIZE,
               "firmware_update_footer_t must be exactly FIRMWARE_FOOTER_SIZE bytes");

#endif  // FIRMWARE_FOOTER_H_
