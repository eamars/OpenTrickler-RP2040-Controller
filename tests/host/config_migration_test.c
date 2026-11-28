/*
    The loader keeps a record an earlier version stored, before fields were appended to its struct (ADR-009, gate 2),
    and a load never writes but for a blank place, another version's record and a save the power cut short (ADR-017,
    gate 1): every other finding leaves the chip as it is and the module on its defaults. A save is whole or not at
    all. A failure would be silent and costly: an update, or one bad read, would reset a user's settings to their
    defaults. load_config and save_config are common.c's own, compiled on the host over an EEPROM in RAM that has a
    real chip's faults put in (fake_eeprom.c; tests/host_test.py).
*/
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "eeprom.h"
#include "fake_eeprom.h"
#include "profile.h"
#include "charge_mode.h"
#include "charge_profiles.h"

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

    // A record damaged since, the spare slot holding another record: nothing read back
    record_v1_t other = {.rev = 0, .a = 1.0f, .b = 2.0f};
    save_config_now(0x7000, &other, sizeof(other));
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


/*
    What a load finds, and what it leaves on the chip (ADR-017, gate 1): each finding put in on the fake chip, the
    struct checked to hold settings to run on, and the chip's bytes checked unchanged wherever the load must not write
*/
#define SPARE_ADDR EEPROM_SPARE_BASE_ADDR

static uint8_t before[EEPROM_SLOT_SIZE];

static void keep_slot(uint16_t addr) {
    memcpy(before, fake_eeprom_bytes() + addr, sizeof(before));
}

static bool slot_unchanged(uint16_t addr) {
    return memcmp(before, fake_eeprom_bytes() + addr, sizeof(before)) == 0;
}

static bool amiss_names(uint16_t addr, config_found_t want) {
    uint16_t addrs[4];
    config_found_t found[4];
    size_t count = config_amiss(addrs, found, 4);
    for (size_t idx = 0; idx < count; idx += 1) {
        if (addrs[idx] == addr) {
            return found[idx] == want;
        }
    }
    return false;
}

// Another record saved, so the spare slot holds that one and not the record a test damages
static void spare_elsewhere(void) {
    record_v1_t other = {.rev = 0, .a = 1.0f, .b = 2.0f};
    save_config_now(0x7000, &other, sizeof(other));
}


static void findings(void) {
    record_v2_t v2;
    record_v2_t full = {.rev = 0, .a = 3.0f, .b = 4.0f, .c = 5.0f, .d = 6.0f};

    // Blank: its defaults, stored
    fake_eeprom_erase();
    config_found_t found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    record_v2_t read_back;
    check(found == CONFIG_FOUND_BLANK && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f) &&
          config_read_stored(ADDR, &read_back, sizeof(read_back)) && config_is_saved(ADDR, &v2, sizeof(v2)) &&
          !amiss_names(ADDR, CONFIG_FOUND_BLANK), "a blank place: its defaults, stored, and nothing said");

    // Damaged: the defaults run, the chip as it was, said, not saved; stored once the module is saved
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    spare_elsewhere();
    fake_eeprom_bytes()[ADDR + 3] ^= 0x5A;
    keep_slot(ADDR);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_DAMAGED && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f) && slot_unchanged(ADDR),
          "a damaged record: the defaults run, the chip left as it is");
    check(!config_is_saved(ADDR, &v2, sizeof(v2)) && amiss_names(ADDR, CONFIG_FOUND_DAMAGED),
          "and it is said: not saved, and amiss");
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_DAMAGED && slot_unchanged(ADDR), "the next start finds it the same, still unwritten");
    check(save_config_now(ADDR, &v2, sizeof(v2)) && config_found(ADDR) == CONFIG_FOUND_READ &&
          !amiss_names(ADDR, CONFIG_FOUND_DAMAGED) && config_is_saved(ADDR, &v2, sizeof(v2)),
          "saved by the person: stored, and no longer amiss");

    // A pre-1.17 record (its revision in the first two bytes): not read, not written
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    spare_elsewhere();
    fake_eeprom_bytes()[ADDR] = 7;
    keep_slot(ADDR);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_DAMAGED && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f) && slot_unchanged(ADDR),
          "a record whose first two bytes are neither 0 nor blank: the defaults, the chip left as it is");

    // Read two ways, both wrong: the bus at fault, said so, nothing written
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    spare_elsewhere();
    fake_eeprom_unsteady(ADDR + 3, true);
    keep_slot(ADDR);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_UNSTEADY && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f) && slot_unchanged(ADDR) &&
          amiss_names(ADDR, CONFIG_FOUND_UNSTEADY), "read two ways: the bus at fault, the defaults, nothing written");

    // The first read wrong, the second whole (the review, finding 3): the record, read at the second try
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    spare_elsewhere();
    fake_eeprom_unsteady(ADDR + 3, false);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 3.0f, 4.0f, 5.0f, 6.0f) && config_is_saved(ADDR, &v2, sizeof(v2)),
          "a first read wrong and a second whole: the record read, not the defaults");

    // Not reachable: every load on its defaults, nothing read or written, a save refused
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    keep_slot(ADDR);
    fake_eeprom_set_reachable(false);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_UNREACHABLE && v2_is(&v2, 10.0f, 20.0f, -1.0f, -1.0f) &&
          !save_config_now(ADDR, &v2, sizeof(v2)) && amiss_names(ADDR, CONFIG_FOUND_UNREACHABLE),
          "an EEPROM that does not answer: the defaults, a save refused, said");
    fake_eeprom_set_reachable(true);
    check(slot_unchanged(ADDR), "and the chip is as it was");

    // A save cut by the power in its place: the spare holds it whole, and the next start finishes it
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    record_v2_t newer = {.rev = 0, .a = 13.0f, .b = 14.0f, .c = 15.0f, .d = 16.0f};
    size_t spare_bytes = 8 + sizeof(newer) + 4 + 4;
    fake_eeprom_cut_after((long) (spare_bytes + 7));        // the spare whole, seven bytes of the place: a byte changed
    check(!save_config_now(ADDR, &newer, sizeof(newer)), "a save cut by the power says it did not store");
    fake_eeprom_cut_after(-1);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_FINISHED && v2_is(&v2, 13.0f, 14.0f, 15.0f, 16.0f) &&
          config_read_stored(ADDR, &read_back, sizeof(read_back)) && v2_is(&read_back, 13.0f, 14.0f, 15.0f, 16.0f),
          "cut in its place: the next start takes the new record whole from the spare, and stores it in its place");

    // A save cut while the spare was written: the place untouched, the record before it read
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    fake_eeprom_cut_after(10);
    check(!save_config_now(ADDR, &newer, sizeof(newer)), "a save cut in the spare slot says it did not store");
    fake_eeprom_cut_after(-1);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 3.0f, 4.0f, 5.0f, 6.0f), "cut in the spare: the last good record read");

    // Every page boundary of a longer record: whatever the cut, one whole record is read, the old or the new
    static eeprom_profile_data_t old_powders, new_powders, got;
    memset(&old_powders, 0, sizeof(old_powders));
    memset(&new_powders, 0, sizeof(new_powders));
    for (int idx = 0; idx < MAX_PROFILE_CNT; idx += 1) {
        snprintf(old_powders.profiles[idx].name, PROFILE_NAME_MAX_LEN, "Old%d", idx);
        snprintf(new_powders.profiles[idx].name, PROFILE_NAME_MAX_LEN, "New%d", idx);
    }
    static const config_layout_t layout = {.header = offsetof(eeprom_profile_data_t, profiles),
                                           .element = sizeof(profile_t), .count = MAX_PROFILE_CNT};
    size_t total = 2 * (8 + sizeof(new_powders) + 4 + 4) ;
    bool whole = true;
    for (long cut = 0; cut <= (long) total; cut += 64) {
        fake_eeprom_erase();
        save_config_now(POWDERS_ADDR, &old_powders, sizeof(old_powders));
        fake_eeprom_cut_after(cut);
        save_config_now(POWDERS_ADDR, &new_powders, sizeof(new_powders));
        fake_eeprom_cut_after(-1);
        found = load_config_layout(POWDERS_ADDR, &got, &old_powders, sizeof(got), &layout);
        bool is_old = memcmp(&got, &old_powders, sizeof(got)) == 0 && found == CONFIG_FOUND_READ;
        bool is_new = memcmp(&got, &new_powders, sizeof(got)) == 0 &&
                      (found == CONFIG_FOUND_READ || found == CONFIG_FOUND_FINISHED);
        if (!is_old && !is_new) {
            printf("  cut after %ld bytes: %s\n", cut, config_found_text(found));
            whole = false;
        }
    }
    check(whole, "the powders cut at every page of their save: the old record or the new, whole, never the defaults");

    // A write the chip does not take, with no word: found by its read back, not believed
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    fake_eeprom_drop(ADDR + 6);
    check(!save_config_now(ADDR, &newer, sizeof(newer)), "a write that does not take is not believed");
    fake_eeprom_drop(-1);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_FINISHED && v2_is(&v2, 13.0f, 14.0f, 15.0f, 16.0f),
          "and the next start takes the record from the spare");

    // On a trial: a damaged record is not held as its defaults, so nothing is written for it when the update is kept
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    spare_elsewhere();
    fake_eeprom_bytes()[ADDR + 3] ^= 0x5A;
    keep_slot(ADDR);
    config_hold_writes(true);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    uint16_t reset[4];
    size_t resets = config_write_held(reset, 4);
    check(found == CONFIG_FOUND_DAMAGED && resets == 0 && slot_unchanged(ADDR),
          "on a trial, a damaged record is not written when the update is kept");

    // On a trial, a held write that fails when the update is kept: not saved, and amiss
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    config_hold_writes(true);
    load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    v2.a = 99.0f;
    save_config(ADDR, &v2, sizeof(v2));
    check(config_is_saved(ADDR, &v2, sizeof(v2)), "on a trial, a write held counts as saved");
    fake_eeprom_set_reachable(false);
    config_write_held(reset, 4);
    fake_eeprom_set_reachable(true);
    check(!config_is_saved(ADDR, &v2, sizeof(v2)) && amiss_names(ADDR, CONFIG_FOUND_NOT_WRITTEN),
          "its write failing when the update is kept: not saved, and amiss, not dropped silently");

    // The review, finding 1: a place that did not take its write, its one whole copy in the spare, is finished into
    // its place before another record's save writes over the spare
    record_v1_t other = {.rev = 0, .a = 1.0f, .b = 2.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    fake_eeprom_drop(ADDR + 6);
    check(!save_config_now(ADDR, &newer, sizeof(newer)), "a place that does not take its write: the save says so");
    fake_eeprom_drop(-1);
    check(save_config_now(0x7000, &other, sizeof(other)), "another record's save goes on");
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 13.0f, 14.0f, 15.0f, 16.0f),
          "and the record before it was finished into its place first: its new settings read, not lost");

    // ... and while its place still does not take it, another record's save is refused, so the copy is kept
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    fake_eeprom_drop(ADDR + 6);
    save_config_now(ADDR, &newer, sizeof(newer));
    check(!save_config_now(0x7000, &other, sizeof(other)),
          "a place that still does not take it: another record's save refused, the spare's copy kept");
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_FINISHED && v2_is(&v2, 13.0f, 14.0f, 15.0f, 16.0f) &&
          !config_is_saved(ADDR, &v2, sizeof(v2)),
          "the next start runs on the spare's copy, and says its place does not hold it (not saved)");
    fake_eeprom_drop(-1);

    // A place that holds a whole record the spare did not write (another firmware's since) keeps it
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    record_v2_t elsewhere_written = {.rev = 0, .a = 21.0f, .b = 22.0f, .c = 23.0f, .d = 24.0f};
    uint32_t crc = software_crc32_continue(0, &elsewhere_written, sizeof(elsewhere_written));
    memcpy(fake_eeprom_bytes() + ADDR, &elsewhere_written, sizeof(elsewhere_written));
    memcpy(fake_eeprom_bytes() + ADDR + sizeof(elsewhere_written), &crc, sizeof(crc));
    fake_eeprom_drop(-1);
    save_config_now(0x7000, &other, sizeof(other));
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 21.0f, 22.0f, 23.0f, 24.0f),
          "a whole record in its place that the spare did not write is kept, not overwritten by the spare's");
}


/*
    The charge profiles' record (ADR-014), by the module's own load (charge_profiles.c): a blank slot's defaults; a
    later version's longer profiles read and stored at this size; and what a record holds that no unit can run on (a
    chosen index or a powder's id out of range, a target that is no weight) run as the first, or not set, in RAM, the
    chip left as it is and the record not stored
*/
#define LOADS_ADDR EEPROM_CHARGE_PROFILES_BASE_ADDR
#define POWDERS 8

typedef struct {
    charge_profile_t now;
    float later;                        // a later version's appended fields
    uint8_t more[4];
} charge_profile_later_t;

typedef struct {
    uint16_t zero;
    uint16_t chosen;
    charge_profile_later_t profiles[CHARGE_PROFILE_COUNT];
} charge_profiles_later_t;


// A record of this version's size, each profile told apart, stored
static void loads_store(eeprom_charge_profile_data_t *record, uint16_t chosen) {
    memset(record, 0, sizeof(*record));
    record->chosen = chosen;
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT; idx += 1) {
        snprintf(record->profiles[idx].name, CHARGE_PROFILE_NAME_LEN, "Stored %d", idx);
        record->profiles[idx].target = 20.0f + (float) idx;
        record->profiles[idx].powder_profile = (uint8_t) (7 - idx);
    }
}


static void loads(void) {
    check(sizeof(charge_profile_t) == 24 && sizeof(eeprom_charge_profile_data_t) == 196 &&
          offsetof(eeprom_charge_profile_data_t, profiles) == 4 && offsetof(charge_profile_t, target) == 16 &&
          offsetof(charge_profile_t, powder_profile) == 20,
          "the charge profiles' record as ADR-014 lays it out: 24 bytes a profile, 196 in all");

    // A blank slot: "Load 1" to "Load 8", no target, each on the powders' stored choice, the first chosen; stored
    fake_eeprom_erase();
    config_found_t found = charge_profiles_load(3, POWDERS);
    bool defaults = found == CONFIG_FOUND_BLANK && charge_profiles_chosen() == 0 && charge_profiles_target() == 0.0f &&
                    charge_profiles_get(CHARGE_PROFILE_COUNT) == NULL;
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT && defaults; idx += 1) {
        char want[CHARGE_PROFILE_NAME_LEN];
        snprintf(want, sizeof(want), "Load %d", idx + 1);
        const charge_profile_t *profile = charge_profiles_get((uint8_t) idx);
        defaults = strcmp(profile->name, want) == 0 && profile->target == 0.0f && profile->powder_profile == 3;
    }
    check(defaults, "a blank slot: Load 1 to Load 8, no target, each on the powder the unit had chosen, the first chosen");
    static eeprom_charge_profile_data_t back;
    check(config_read_stored(LOADS_ADDR, &back, sizeof(back)) && back.zero == 0 && back.chosen == 0 &&
          strcmp(back.profiles[7].name, "Load 8") == 0 && back.profiles[7].powder_profile == 3 &&
          charge_profiles_is_saved() && !amiss_names(LOADS_ADDR, CONFIG_FOUND_BLANK),
          "and stored: its first two bytes 0, saved, nothing said");
    found = charge_profiles_load(5, POWDERS);
    check(found == CONFIG_FOUND_READ && charge_profiles_get(0)->powder_profile == 3 && charge_profiles_is_saved(),
          "the next start reads it: the powders' stored choice is taken for the defaults alone");

    // The powders' stored choice past the last powder: the defaults on the first
    fake_eeprom_erase();
    charge_profiles_load(200, POWDERS);
    check(charge_profiles_get(0)->powder_profile == 0 && charge_profiles_get(7)->powder_profile == 0,
          "a blank slot with the powders' stored choice out of range: each on the first powder");

    // A later version's record, fields appended to each profile: this version's fields read, stored at this size
    static charge_profiles_later_t later;
    memset(&later, 0, sizeof(later));
    later.chosen = 5;
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT; idx += 1) {
        snprintf(later.profiles[idx].now.name, CHARGE_PROFILE_NAME_LEN, "Later %d", idx);
        later.profiles[idx].now.target = 40.0f + 0.25f * (float) idx;
        later.profiles[idx].now.powder_profile = (uint8_t) idx;
        later.profiles[idx].later = 99.0f;
        memset(later.profiles[idx].more, 0xA5, sizeof(later.profiles[idx].more));
    }
    fake_eeprom_erase();
    save_config_now(LOADS_ADDR, &later, sizeof(later));
    found = charge_profiles_load(3, POWDERS);
    bool kept = found == CONFIG_FOUND_MIGRATED && charge_profiles_chosen() == 5 &&
                charge_profiles_target() == 40.0f + 0.25f * 5.0f;
    for (int idx = 0; idx < CHARGE_PROFILE_COUNT && kept; idx += 1) {
        kept = memcmp(charge_profiles_get((uint8_t) idx), &later.profiles[idx].now, sizeof(charge_profile_t)) == 0;
    }
    check(kept, "a record with longer profiles: each read for this version's fields, the choice too");
    check(config_read_stored(LOADS_ADDR, &back, sizeof(back)) && back.chosen == 5 &&
          memcmp(&back.profiles[6], &later.profiles[6].now, sizeof(charge_profile_t)) == 0 &&
          charge_profiles_is_saved(), "and stored at this version's size");

    // A chosen index past the eight, under a good CRC: the first runs in RAM, the chip as it was, not stored
    static eeprom_charge_profile_data_t record;
    loads_store(&record, 9);
    fake_eeprom_erase();
    save_config_now(LOADS_ADDR, &record, sizeof(record));
    keep_slot(LOADS_ADDR);
    found = charge_profiles_load(3, POWDERS);
    check(found == CONFIG_FOUND_READ && charge_profiles_chosen() == 0 && charge_profiles_target() == 20.0f &&
          strcmp(charge_profiles_get(4)->name, "Stored 4") == 0,
          "a chosen index out of range: the first runs in its place, the eight as stored");
    check(slot_unchanged(LOADS_ADDR) && !charge_profiles_is_saved(), "in RAM only: the chip left as it is, not stored");

    // A powder's id past the last powder, a target that is no weight, a name with no ending: the same
    loads_store(&record, 2);
    record.profiles[2].powder_profile = 200;
    record.profiles[3].target = NAN;
    record.profiles[4].target = -5.0f;
    memset(record.profiles[5].name, 'x', CHARGE_PROFILE_NAME_LEN);
    fake_eeprom_erase();
    save_config_now(LOADS_ADDR, &record, sizeof(record));
    keep_slot(LOADS_ADDR);
    found = charge_profiles_load(3, POWDERS);
    check(found == CONFIG_FOUND_READ && charge_profiles_chosen() == 2 && charge_profiles_get(2)->powder_profile == 0 &&
          charge_profiles_get(2)->target == 22.0f && charge_profiles_get(1)->powder_profile == 6,
          "a powder's id out of range: the first powder runs in its place, the rest as stored");
    check(charge_profiles_get(3)->target == 0.0f && charge_profiles_get(4)->target == 0.0f &&
          strlen(charge_profiles_get(5)->name) == CHARGE_PROFILE_NAME_LEN - 1,
          "a target that is no weight is not set, and a name with no ending is ended");
    check(slot_unchanged(LOADS_ADDR) && !charge_profiles_is_saved(), "in RAM only: the chip left as it is, not stored");
    check(charge_profiles_save() && charge_profiles_is_saved() && config_read_stored(LOADS_ADDR, &back, sizeof(back)) &&
          back.profiles[2].powder_profile == 0, "stored by the person: what runs is what the chip holds");

    // Damaged: the defaults run, so no target is set and a start is refused; the chip left as it is, and it is said
    loads_store(&record, 1);
    fake_eeprom_erase();
    save_config_now(LOADS_ADDR, &record, sizeof(record));
    spare_elsewhere();
    fake_eeprom_bytes()[LOADS_ADDR + 21] ^= 0x5A;
    keep_slot(LOADS_ADDR);
    found = charge_profiles_load(3, POWDERS);
    check(found == CONFIG_FOUND_DAMAGED && charge_profiles_chosen() == 0 && charge_profiles_target() == 0.0f &&
          strcmp(charge_profiles_get(1)->name, "Load 2") == 0 && slot_unchanged(LOADS_ADDR) &&
          amiss_names(LOADS_ADDR, CONFIG_FOUND_DAMAGED) && !charge_profiles_is_saved(),
          "a damaged record: the defaults run with no target set, the chip left as it is, and it is said");
}


/*
    The EEPROM's size, told without writing (ADR-014): a blank chip tells nothing; a 128 kbit chip gives at 17 KB what
    lies at 1 KB. The 256 kbit answer is every bench unit's own; the 128 kbit one no board at hand can give, so it is
    the one made here
*/
static void eeprom_size(void) {
    fake_eeprom_erase();
    check(config_eeprom_kb() == 0, "a chip with no record at 1 KB: its size not told");
    record_v2_t full = {.rev = 0, .a = 3.0f, .b = 4.0f, .c = 5.0f, .d = 6.0f};
    save_config_now(EEPROM_SCALE_CONFIG_BASE_ADDR, &full, sizeof(full));
    keep_slot(EEPROM_SCALE_CONFIG_BASE_ADDR);
    // A chip of 16 KB: every address past it reads what lies 16 KB below
    memcpy(fake_eeprom_bytes() + 16 * 1024, fake_eeprom_bytes(), 16 * 1024);
    check(config_eeprom_kb() == 16 && slot_unchanged(EEPROM_SCALE_CONFIG_BASE_ADDR),
          "the same bytes at 1 KB and at 17 KB: one chip of 16 KB, nothing written");
}


int main(void) {
    record_v2_t v2;
    uint16_t reset[4];

    // An update from the earlier version: its fields kept, the appended ones their defaults, stored at the new size
    store_v1();
    config_found_t found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_MIGRATED && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f),
          "an earlier version's record is kept, the appended fields default");
    record_v2_t read_back;
    check(config_read_stored(ADDR, &read_back, sizeof(read_back)) && v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f),
          "it is stored again at the new size");
    check(config_is_saved(ADDR, &v2, sizeof(v2)), "and counts as saved");

    // The next start reads it as it is
    memset(&v2, 0, sizeof(v2));
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "the next start reads the new record as it is");

    // A record of the new size, its appended fields set, is read whole
    record_v2_t full = {.rev = 0, .a = 3.0f, .b = 4.0f, .c = 5.0f, .d = 6.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &full, sizeof(full));
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_READ && v2_is(&v2, 3.0f, 4.0f, 5.0f, 6.0f), "a record of the new size is read whole");

    // On an update's trial: kept in RAM, nothing written until it is confirmed, and not reported as a reset
    store_v1();
    config_hold_writes(true);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_MIGRATED && v2_is(&v2, 1.5f, 2.5f, -1.0f, -1.0f), "on trial, the earlier record is kept in RAM");
    record_v1_t still;
    check(config_read_stored(ADDR, &still, sizeof(still)) && still.a == 1.5f && still.b == 2.5f &&
          !config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "on trial, the EEPROM still holds the earlier record, so going back reads it");
    size_t resets = config_write_held(reset, 4);
    check(resets == 0 && config_read_stored(ADDR, &read_back, sizeof(read_back)) &&
          v2_is(&read_back, 1.5f, 2.5f, -1.0f, -1.0f), "confirmed, it is written at the new size and no reset is said");

    // On a trial, a blank place: held as its defaults, and said reset when the update is kept
    fake_eeprom_erase();
    config_hold_writes(true);
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    resets = config_write_held(reset, 4);
    check(found == CONFIG_FOUND_BLANK && resets == 1 && reset[0] == ADDR && config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "on trial, a blank place is held as its defaults and said reset when kept");

    // A later version's record, its fields appended after ours (the owner, 2026-11-26: forward compatibility): ours
    // read, the rest discarded, and stored at once at this version's size
    record_v2_t later = {.rev = 0, .a = 7.0f, .b = 8.0f, .c = 9.0f, .d = 10.0f};
    fake_eeprom_erase();
    save_config_now(ADDR, &later, sizeof(later));
    record_v1_t v1;
    found = load_config(ADDR, &v1, &defaults_v1, sizeof(v1));
    check(found == CONFIG_FOUND_MIGRATED && v1.rev == 0 && v1.a == 7.0f && v1.b == 8.0f,
          "a later version's longer record: our fields read");
    record_v1_t v1_back;
    check(config_read_stored(ADDR, &v1_back, sizeof(v1_back)) && v1_back.a == 7.0f && v1_back.b == 8.0f &&
          !config_read_stored(ADDR, &read_back, sizeof(read_back)),
          "stored at once at our size, the later version's fields discarded");
    check(config_is_saved(ADDR, &v1, sizeof(v1)), "and it counts as saved");

    // The later version again: ours read back, its own at their defaults
    found = load_config(ADDR, &v2, &defaults_v2, sizeof(v2));
    check(found == CONFIG_FOUND_MIGRATED && v2_is(&v2, 7.0f, 8.0f, -1.0f, -1.0f),
          "the later version reads them back, its own at their defaults");

    // A damaged longer record matches at no size, the spare holding another record: the defaults, the chip unwritten
    fake_eeprom_erase();
    save_config_now(ADDR, &later, sizeof(later));
    spare_elsewhere();
    fake_eeprom_bytes()[ADDR + 5] ^= 0x5A;
    keep_slot(ADDR);
    found = load_config(ADDR, &v1, &defaults_v1, sizeof(v1));
    check(found == CONFIG_FOUND_DAMAGED && v1.a == 10.0f && v1.b == 20.0f && slot_unchanged(ADDR),
          "a damaged longer record gets the defaults, the chip left as it is");

    findings();
    powders();
    charge_record();
    loads();
    eeprom_size();

    printf("%s: %d failed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
