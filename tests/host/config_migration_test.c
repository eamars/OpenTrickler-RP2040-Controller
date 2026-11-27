/*
    The loader keeps a record an earlier version stored, before fields were appended to its struct (ADR-009, gate 2).
    A failure would be silent and costly: an update would reset a user's settings to their defaults. load_config and
    save_config are common.c's own, compiled on the host over an EEPROM in RAM (tests/host_test.py).
*/
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "fake_eeprom.h"
#include "profile.h"
#include "charge_mode.h"

#define ADDR 0x0400

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
static const record_v1_t defaults_v1 = {.rev = 0, .a = 10.0f, .b = 20.0f};
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


/*
    A record that is a header and an array, grown at each element's end (load_config_layout): v1.21's powders, as it
    stored them, read by this version's profile_t (two weights a turn appended); and a later version's, one more field
    appended to each powder, read by this one
*/
typedef struct {
    uint32_t rev;
    uint32_t compatibility;
    char name[PROFILE_NAME_MAX_LEN];
    float coarse_kp, coarse_ki, coarse_kd, coarse_min, coarse_max;
    float fine_kp, fine_ki, fine_kd, fine_min, fine_max;
} profile_v121_t;                       // v1.21's src/services/profile/include/profile.h

typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;
    profile_v121_t profiles[MAX_PROFILE_CNT];
} profiles_v121_t;

typedef struct {
    profile_t now;
    float later;                        // a later version's appended field
} profile_later_t;

typedef struct {
    uint16_t profile_data_rev;
    uint16_t current_profile_idx;
    profile_later_t profiles[MAX_PROFILE_CNT];
} profiles_later_t;

#define POWDERS_ADDR 0x2400

static const config_layout_t powders_layout = {.header = offsetof(eeprom_profile_data_t, profiles),
                                               .element = sizeof(profile_t), .count = MAX_PROFILE_CNT};


static void powders(void) {
    static eeprom_profile_data_t defaults;
    for (int idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        snprintf(defaults.profiles[idx].name, PROFILE_NAME_MAX_LEN, "Default%d", idx);
        defaults.profiles[idx].fine_weight_per_turn = -2.0f;      // told apart from what was stored
        defaults.profiles[idx].coarse_weight_per_turn = -3.0f;
    }

    // v1.21's eight powders: every field it had kept, the appended ones at their defaults, stored at the new size
    static profiles_v121_t old;
    memset(&old, 0, sizeof(old));
    old.current_profile_idx = 3;
    for (int idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        snprintf(old.profiles[idx].name, PROFILE_NAME_MAX_LEN, "Powder%d", idx);
        old.profiles[idx].compatibility = (uint32_t) idx;
        old.profiles[idx].coarse_kp = 0.01f * (float) (idx + 1);
        old.profiles[idx].fine_kp = 0.5f + (float) idx;
        old.profiles[idx].fine_max = 5.0f - (float) idx * 0.25f;
    }
    fake_eeprom_erase();
    save_config_now(POWDERS_ADDR, &old, sizeof(old));
    static eeprom_profile_data_t now;
    bool ok = load_config_layout(POWDERS_ADDR, &now, &defaults, sizeof(now), &powders_layout);
    bool kept = ok && now.current_profile_idx == 3;
    for (int idx = 0; idx < MAX_PROFILE_CNT && kept; idx += 1) {
        const profile_t *p = &now.profiles[idx];
        char want[PROFILE_NAME_MAX_LEN];
        snprintf(want, sizeof(want), "Powder%d", idx);
        kept = strcmp(p->name, want) == 0 && p->compatibility == (uint32_t) idx &&
               p->coarse_kp == 0.01f * (float) (idx + 1) && p->fine_kp == 0.5f + (float) idx &&
               p->fine_max_flow_speed_rps == 5.0f - (float) idx * 0.25f &&
               p->fine_weight_per_turn == -2.0f && p->coarse_weight_per_turn == -3.0f;
    }
    check(kept, "v1.21's powders: each kept whole, the choice too, the weights a turn at their defaults");
    static eeprom_profile_data_t back;
    check(config_read_stored(POWDERS_ADDR, &back, sizeof(back)) && memcmp(&back, &now, sizeof(now)) == 0,
          "and stored at the new size");

    // A part read back as stored (the review's S1): a powder as this version holds it, not the raw bytes at its offset
    profile_t part;
    size_t second = offsetof(eeprom_profile_data_t, profiles) + sizeof(profile_t);
    check(config_read_part(POWDERS_ADDR + second, &part, sizeof(part)) && memcmp(&part, &now.profiles[1], sizeof(part)) == 0,
          "a powder read back as stored, at this version's offsets");

    // On an update's trial the EEPROM still holds v1.21's block: the part read back is the write held
    fake_eeprom_erase();
    save_config_now(POWDERS_ADDR, &old, sizeof(old));
    config_hold_writes(true);
    ok = load_config_layout(POWDERS_ADDR, &now, &defaults, sizeof(now), &powders_layout);
    check(ok && config_read_part(POWDERS_ADDR + second, &part, sizeof(part)) &&
          memcmp(&part, &now.profiles[1], sizeof(part)) == 0 && strcmp(part.name, "Powder1") == 0,
          "on a trial, a powder read back is the write held, not v1.21's bytes");
    uint16_t trial_reset[4];
    config_write_held(trial_reset, 4);

    // A record damaged since: nothing read back
    fake_eeprom_bytes()[POWDERS_ADDR + 10] ^= 0x5A;
    check(!config_read_part(POWDERS_ADDR + second, &part, sizeof(part)), "a damaged record reads nothing back");

    // A later version's powders, a field appended to each: this one's fields read, the rest discarded
    static profiles_later_t later;
    memset(&later, 0, sizeof(later));
    later.current_profile_idx = 5;
    for (int idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        later.profiles[idx].now = now.profiles[idx];
        later.profiles[idx].now.fine_weight_per_turn = 0.8f + 0.01f * (float) idx;
        later.profiles[idx].now.coarse_weight_per_turn = 10.0f + (float) idx;
        later.profiles[idx].later = 99.0f;
    }
    fake_eeprom_erase();
    save_config_now(POWDERS_ADDR, &later, sizeof(later));
    ok = load_config_layout(POWDERS_ADDR, &now, &defaults, sizeof(now), &powders_layout);
    kept = ok && now.current_profile_idx == 5;
    for (int idx = 0; idx < MAX_PROFILE_CNT && kept; idx += 1) {
        kept = memcmp(&now.profiles[idx], &later.profiles[idx].now, sizeof(profile_t)) == 0;
    }
    check(kept, "a later version's powders: each read for this version's fields");
    check(config_read_stored(POWDERS_ADDR, &back, sizeof(back)) && memcmp(&back, &now, sizeof(now)) == 0,
          "and stored at this version's size");
}


/*
    The charge record, whose history had insertions (ADR-009's review, B1): v1.21's 52 bytes, as its build stored them,
    read field by field through charge_mode_layout; a record of a layout before v1.21 (48 bytes) gets the defaults,
    never a shifted prefix
*/
#define CHARGE_ADDR 0x1400

static void put_float(uint8_t *bytes, size_t at, float value) {
    memcpy(bytes + at, &value, sizeof(value));
}


static void put_u32(uint8_t *bytes, size_t at, uint32_t value) {
    memcpy(bytes + at, &value, sizeof(value));
}


static uint32_t colour_of(const rgbw_u32_t *colour) {
    uint32_t value;
    memcpy(&value, colour, sizeof(value));
    return value;
}


static void charge_record(void) {
    static eeprom_charge_mode_data_t defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.coarse_stop_threshold = 5.0f;
    defaults.coarse_stop_backoff_enable = false;
    defaults.coarse_stop_backoff_turns = 0.25f;
    defaults.coarse_stop_backoff_speed_rps = 1.0f;
    defaults.precharge_time_ms = 1000;
    defaults.cup_capacity = -1.0f;
    defaults.cup_weight = -1.0f;
    defaults.auto_tune = false;
    defaults.forecast = false;

    // v1.21's record as its own build laid it out, each field a value of its own
    uint8_t v121[52];
    memset(v121, 0, sizeof(v121));
    put_float(v121, 4, 7.5f);           // coarse_stop_threshold
    put_float(v121, 8, 0.04f);          // fine_stop_threshold
    put_float(v121, 12, 0.03f);         // set_point_sd_margin
    put_float(v121, 16, 0.025f);        // set_point_mean_margin
    put_float(v121, 20, 0.6f);          // coarse_stop_gate_ratio
    v121[24] = 1;                       // decimal_places
    v121[25] = 1;                       // precharge_enable
    put_u32(v121, 28, 1500);            // precharge_time_ms
    put_float(v121, 32, 1.5f);          // precharge_speed_rps
    put_u32(v121, 36, 0x00112233u);     // the four colours
    put_u32(v121, 40, 0x00445566u);
    put_u32(v121, 44, 0x00778899u);
    put_u32(v121, 48, 0x00aabbccu);
    fake_eeprom_erase();
    save_config_now(CHARGE_ADDR, v121, sizeof(v121));

    static eeprom_charge_mode_data_t charge;
    bool ok = load_config_layout(CHARGE_ADDR, &charge, &defaults, sizeof(charge), &charge_mode_layout);
    check(ok && charge.coarse_stop_threshold == 7.5f && charge.fine_stop_threshold == 0.04f &&
          charge.set_point_sd_margin == 0.03f && charge.set_point_mean_margin == 0.025f &&
          charge.coarse_stop_gate_ratio == 0.6f && charge.decimal_places == (decimal_places_t) 1 &&
          charge.precharge_enable && charge.precharge_time_ms == 1500 && charge.precharge_speed_rps == 1.5f &&
          colour_of(&charge.neopixel_normal_charge_colour) == 0x00112233u &&
          colour_of(&charge.neopixel_under_charge_colour) == 0x00445566u &&
          colour_of(&charge.neopixel_over_charge_colour) == 0x00778899u &&
          colour_of(&charge.neopixel_not_ready_colour) == 0x00aabbccu,
          "v1.21's charge record: every field it had read from its own place");
    check(!charge.coarse_stop_backoff_enable && charge.coarse_stop_backoff_turns == 0.25f &&
          charge.coarse_stop_backoff_speed_rps == 1.0f && charge.cup_capacity == -1.0f && charge.cup_weight == -1.0f &&
          !charge.auto_tune && !charge.forecast, "the fields v2 added at their defaults: the new features off");
    static eeprom_charge_mode_data_t back;
    check(config_read_stored(CHARGE_ADDR, &back, sizeof(back)) && memcmp(&back, &charge, sizeof(back)) == 0,
          "and stored at this version's size");
    memset(&charge, 0, sizeof(charge));
    ok = load_config_layout(CHARGE_ADDR, &charge, &defaults, sizeof(charge), &charge_mode_layout);
    check(ok && memcmp(&charge, &back, sizeof(back)) == 0, "the next start reads it whole");

    // A layout before v1.21 (v1.16 to v1.18's 48 bytes): not read as a prefix, the defaults
    uint8_t v118[48];
    memcpy(v118, v121, sizeof(v118));
    fake_eeprom_erase();
    save_config_now(CHARGE_ADDR, v118, sizeof(v118));
    ok = load_config_layout(CHARGE_ADDR, &charge, &defaults, sizeof(charge), &charge_mode_layout);
    check(ok && memcmp(&charge, &defaults, sizeof(charge)) == 0, "a layout before v1.21 gets the defaults, not a shifted prefix");
}


int main(void) {
    record_v2_t v2;
    uint16_t reset[4];

    // An update from the earlier version: its fields kept, the appended ones their defaults, stored at the new size
    store_v1();
    bool ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "an earlier version's record is kept, the appended fields default");
    record_v2_t read_back;
    check(config_read_stored(ADDR, &read_back, sizeof(read_back)) && v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f),
          "it is stored again at the new size");
    check(config_is_saved(ADDR, &v2, sizeof(v2)), "and counts as saved");

    // The next start reads it as it is
    memset(&v2, 0, sizeof(v2));
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "the next start reads the new record as it is");

    // A record of the new size, its appended fields set, is read whole
    record_v2_t full = {.rev = 0, .a = 3.0f, .b = 4.0f, .c = 5.0f, .d = 6.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 3.0f, 4.0f, 5.0f, 6.0f), "a record of the new size is read whole");

    // A damaged record matches at no size: the defaults, as today
    store_v1();
    fake_eeprom_bytes()[ADDR + 3] ^= 0x5A;
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f), "a damaged record gets the defaults");

    // A new chip, erased: the defaults
    fake_eeprom_erase();
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f), "an erased EEPROM gets the defaults");

    // On an update's trial: kept in RAM, nothing written until it is confirmed, and not reported as a reset
    store_v1();
    config_hold_writes(true);
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "on trial, the earlier record is kept in RAM");
    record_v1_t still;
    check(config_read_stored(ADDR, &still, sizeof(still)) && still.a == 1.5f && still.b == 2.5f &&
          !config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "on trial, the EEPROM still holds the earlier record, so going back reads it");
    check(!config_defaults_held(ADDR), "on trial, it is not held as a reset");
    size_t resets = config_write_held(reset, 4);
    check(resets == 0 && config_read_stored(ADDR, &read_back, sizeof(read_back)) &&
          v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f), "confirmed, it is written at the new size and no reset is said");

    // A later version's record, its fields appended after ours (the owner, 2026-11-26: forward compatibility): ours
    // read, the rest discarded, and stored at once at this version's size
    record_v2_t later = {.rev = 0, .a = 7.0f, .b = 8.0f, .c = 9.0f, .d = 10.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &later, sizeof(later));
    record_v1_t v1;
    ok = load_config(ADDR, &v1, &defaults_v1, sizeof(v1));
    check(ok && v1.rev == 0 && v1.a == 7.0f && v1.b == 8.0f, "a later version's longer record: our fields read");
    record_v1_t v1_back;
    check(config_read_stored(ADDR, &v1_back, sizeof(v1_back)) && v1_back.a == 7.0f && v1_back.b == 8.0f &&
          !config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "stored at once at our size, the later version's fields discarded");
    check(config_is_saved(ADDR, &v1, sizeof(v1)), "and it counts as saved");

    // The later version again: ours read back, its own fields at their defaults
    ok = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(ok && v2_is(&v2, 7.0f, 8.0f, -1.0f, -1.0f), "the later version reads them back, its own at their defaults");

    // A damaged longer record matches at no size: the defaults
    fake_eeprom_erase();
    save_config_now(ADDR, &later, sizeof(later));
    fake_eeprom_bytes()[ADDR + 5] ^= 0x5A;
    ok = load_config(ADDR, &v1, &defaults_v1, sizeof(v1));
    check(ok && v1.a == 10.0f && v1.b == 20.0f, "a damaged longer record gets the defaults");

    powders();
    charge_record();

    printf("%s: %d failed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
