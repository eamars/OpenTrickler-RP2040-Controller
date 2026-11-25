/*
    The loader keeps a record an earlier version stored, before fields were appended to its struct (ADR-009, gate 2).
    A failure would be silent and costly: an update would reset a user's settings to their defaults. load_config and
    save_config are common.c's own, compiled on the host over an EEPROM in RAM (tests/host_test.py).
*/
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "fake_eeprom.h"

#define ADDR 0x0400
#define REV 7               // an old revision number no record carries

typedef struct {
    uint16_t rev;
    float a;
    float b;
} record_v1_t;              // as an earlier version stored it

typedef struct {
    uint16_t rev;
    float a;
    float b;
    float c;                // appended
    float d;                // appended
} record_v2_t;

static const record_v1_t stored_v1 = {.rev = 0, .a = 1.5f, .b = 2.5f};
static const record_v2_t defaults_v2 = {.rev = 0, .a = 10.0f, .b = 20.0f, .c = -1.0f, .d = -1.0f};
static int failures = 0;


static void check(bool ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    failures += ok ? 0 : 1;
}


static bool v2_is(const record_v2_t *r, float a, float b, float c, float d) {
    return r->rev == 0 && r->a == a && r->b == b && r->c == c && r->d == d;
}


static void store_v1(void) {
    fake_eeprom_erase();
    record_v1_t v1 = stored_v1;
    save_config_now(ADDR, &v1, sizeof(v1));
}


int main(void) {
    record_v2_t v2;
    uint16_t reset[4];

    // An update from the earlier version: its fields kept, the appended ones their defaults, stored at the new size
    store_v1();
    bool ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "an earlier version's record is kept, the appended fields default");
    record_v2_t read_back;
    check(config_read_stored(ADDR, &read_back, sizeof(read_back)) && v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f),
          "it is stored again at the new size");
    check(config_is_saved(ADDR, &v2, sizeof(v2)), "and counts as saved");

    // The next start reads it as it is
    memset(&v2, 0, sizeof(v2));
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "the next start reads the new record as it is");

    // A record of the new size, its appended fields set, is read whole
    record_v2_t full = {.rev = 0, .a = 3.0f, .b = 4.0f, .c = 5.0f, .d = 6.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 3.0f, 4.0f, 5.0f, 6.0f), "a record of the new size is read whole");

    // A damaged record matches at no size: the defaults, as today
    store_v1();
    fake_eeprom_bytes()[ADDR + 3] ^= 0x5A;
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f), "a damaged record gets the defaults");

    // A new chip, erased: the defaults
    fake_eeprom_erase();
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f), "an erased EEPROM gets the defaults");

    // On an update's trial: kept in RAM, nothing written until it is confirmed, and not reported as a reset
    store_v1();
    config_hold_writes(true);
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2), REV);
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "on trial, the earlier record is kept in RAM");
    record_v1_t still;
    check(config_read_stored(ADDR, &still, sizeof(still)) && still.a == 1.5f && still.b == 2.5f &&
          !config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "on trial, the EEPROM still holds the earlier record, so going back reads it");
    check(!config_defaults_held(ADDR), "on trial, it is not held as a reset");
    size_t resets = config_write_held(reset, 4);
    check(resets == 0 && config_read_stored(ADDR, &read_back, sizeof(read_back)) &&
          v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f), "confirmed, it is written at the new size and no reset is said");

    printf("%s: %d failed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
