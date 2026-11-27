#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mbedtls/sha256.h"
#include "pico/unique_id.h"

#include "unit_id.h"

// Letters and digits with none that the screen's font draws alike or nearly: no i, l, o, 0 or 1. 31 of them, so
// eight carry some 40 bits
static const char unit_key_alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";


// SHA-256 of a word and the chip's unique id: each word gives a hash that tells nothing of another word's
static void unit_id_hash(const char *word, uint8_t digest[32]) {
    pico_unique_board_id_t chip;
    pico_get_unique_board_id(&chip);

    uint8_t input[32 + PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    size_t word_len = strlen(word);
    memcpy(input, word, word_len);
    memcpy(input + word_len, chip.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);
    mbedtls_sha256(input, word_len + PICO_UNIQUE_BOARD_ID_SIZE_BYTES, digest, 0);
}


void unit_id_get(char *id, size_t room) {
    uint8_t digest[32];
    unit_id_hash("opentrickler unit id", digest);
    snprintf(id, room, "%02X%02X%02X%02X", digest[0], digest[1], digest[2], digest[3]);
}


void unit_id_hotspot_key(char *key, size_t room) {
    uint8_t digest[32];
    unit_id_hash("opentrickler hotspot key", digest);

    // The hash's first 64 bits written in base 31: eight digits take some 40 of them, and the bias of the last digit
    // is some 2^-24, none to speak of
    uint64_t value = 0;
    for (size_t b = 0; b < 8; b++) {
        value = (value << 8) | digest[b];
    }
    size_t length = room - 1 < UNIT_HOTSPOT_KEY_LENGTH ? room - 1 : UNIT_HOTSPOT_KEY_LENGTH;
    for (size_t i = 0; i < length; i++) {
        key[i] = unit_key_alphabet[value % (sizeof(unit_key_alphabet) - 1)];
        value /= sizeof(unit_key_alphabet) - 1;
    }
    key[length] = '\0';
}
