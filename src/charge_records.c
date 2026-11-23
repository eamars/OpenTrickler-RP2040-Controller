/*
 * What this unit measured, stored (ADR-009, gate R; charge_records.h).
 */
#include "charge_records.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>

#include "activity.h"
#include "common.h"
#include "eeprom.h"
#include "events.h"
#include "settings.h"
#include "unit_id.h"

#define CHARGE_RECORD_SLOT_SIZE 1024
#define CHARGE_RECORD_PROFILES 8
#define CHARGE_RECORD_LAW_STOPS_OLD_ADDR (21 * 1024)    // before gate R: the PID law's own lists, now zeroed
#define CHARGE_RECORD_LAW_STOPS_OLD_SIZE (2 + 8 + 8 * 16 * 4 + 2 + 4)  // that record, its padding and CRC: 528 B
#define CHARGE_RECORD_NOT_MEASURED (-1.0f)              // a scale's lag or rest no calibration has measured

// --- The stored forms. Their layout is the stored format: changed only with its version, and declared in ADR-009

typedef struct {
    float speed_rps;
    float unseen_gr;
    float spread_gr;
    float quiet_s;
    float lag_s;            // below zero: not measured
} charge_record_stop_t;

typedef struct {
    float turns;
    float mean_gr;
    float most_gr;
    float wait_s;
} charge_record_move_t;

typedef struct {
    float error_gr;
} charge_record_error_t;

// Each list: its count in a byte, then its rows (settings.h's list kind)
typedef struct { uint8_t count; uint8_t reserved[3]; charge_record_stop_t rows[CHARGE_RECORD_COARSE_STOPS]; } coarse_list_t;
typedef struct { uint8_t count; uint8_t reserved[3]; charge_record_stop_t rows[CHARGE_RECORD_FINE_STOPS]; } fine_list_t;
typedef struct { uint8_t count; uint8_t reserved[3]; charge_record_move_t rows[CHARGE_RECORD_MOVES]; } move_list_t;
typedef struct { uint8_t count; uint8_t reserved[3]; charge_record_error_t rows[CHARGE_RECORD_ERRORS]; } error_list_t;

typedef struct {
    uint16_t legacy_rev;            // load_config's old revision check: 0
    uint8_t version;                // CHARGE_RECORD_VERSION
    uint8_t held;                   // a calibration is held
    char unit_id[CHARGE_RECORD_UNIT_ID_ROOM];   // the unit it was measured on
    float coarse_gr_turn;
    float fine_gr_turn;
    float coarse_share;
    float watch_s;
    uint8_t law;                    // the law that learnt the carried stops: 0 the glide, 1 the PID law
    uint8_t reserved[3];
    coarse_list_t coarse_stops;
    fine_list_t fine_stops;
    move_list_t moves;
    error_list_t first_errors;      // the carried stops: the rehearsals', then the charges'
} charge_powder_record_t;

typedef struct {
    uint16_t legacy_rev;
    uint8_t version;
    uint8_t held;
    char unit_id[CHARGE_RECORD_UNIT_ID_ROOM];
    float digit_gr;
    float interval_s;               // for the simulation; the firmware takes the scale's live rhythm (B2)
    float scale_lag_s;              // CHARGE_RECORD_NOT_MEASURED until gate 3 measures it
    float scale_rest_s;
} charge_unit_record_t;

_Static_assert(sizeof(charge_powder_record_t) + sizeof(uint32_t) <= CHARGE_RECORD_SLOT_SIZE, "a record fits its slot");
_Static_assert(sizeof(charge_powder_record_t) <= SETTINGS_STRUCT_MAX, "a record is a settings resource");
_Static_assert(sizeof(charge_calibration_record_t) == 420, "the v1 record's size, as stored before gate R");

static charge_unit_record_t charge_unit_record;
static charge_powder_record_t *charge_records_cache = NULL;    // the API's copy of one powder's record, from the heap
static int8_t charge_records_cache_id = -1;
// The cache holds a write not yet stored (staged): a write takes effect at its save, as every setting is stored at
// its save, and a charge reads only what is stored. Lost on a restart, as every live setting is
static bool charge_records_staged = false;
#define RECORDS_POWDER_PATH "/api/v1/settings/powder_records/{id}"
#define RECORDS_POWDER_SAVE "/api/v1/settings/powder_records/save"
#define RECORDS_POWDER_REVERT "/api/v1/settings/powder_records/revert"


// A powder's record written by the unit itself (a charge's end, the calibration, the console): the API's copy of it
// is read again. A write of the API's staged for it is dropped, and said: the unit's own write is the newer
static void charge_records_forget(uint8_t profile) {
    if (charge_records_cache_id != (int8_t) profile) {
        return;
    }
    if (charge_records_staged) {
        charge_records_staged = false;
        printf("Records: powder_records/%u's write not saved is dropped: the unit wrote the record itself\n", profile);
        event_t event = {.source = EVENT_SOURCE_SETTING, .kind = EVENT_DROPPED, .by = ACTIVITY_BY_UNIT,
                         .text = RECORDS_POWDER_PATH, .number = profile};
        events_add(&event);
    }
    charge_records_cache_id = -1;
}


static uint16_t charge_records_addr(uint8_t profile) {
    return (uint16_t) (EEPROM_CHARGE_CALIBRATION_BASE_ADDR + profile * CHARGE_RECORD_SLOT_SIZE);
}


static void charge_records_this_unit(char *out) {
    memset(out, 0, CHARGE_RECORD_UNIT_ID_ROOM);
    unit_id_get(out, CHARGE_RECORD_UNIT_ID_ROOM);
}


// Nothing stored: an erased EEPROM's 0xff, or zeros
static bool charge_records_blank(uint16_t addr) {
    uint8_t head[8];
    if (!eeprom_read(addr, head, sizeof(head))) {
        return false;
    }
    bool ones = true, zeros = true;
    for (size_t i = 0; i < sizeof(head); i++) {
        ones = ones && head[i] == 0xff;
        zeros = zeros && head[i] == 0x00;
    }
    return ones || zeros;
}


static charge_record_state_t charge_records_read(uint8_t profile, charge_powder_record_t *out) {
    uint16_t addr = charge_records_addr(profile);
    if (config_read_stored(addr, out, sizeof(*out)) && out->version == CHARGE_RECORD_VERSION) {
        return out->held ? CHARGE_RECORD_OK : CHARGE_RECORD_NONE;
    }
    return charge_records_blank(addr) ? CHARGE_RECORD_NONE : CHARGE_RECORD_DAMAGED;
}


static bool charge_records_write(uint8_t profile, charge_powder_record_t *record) {
    record->legacy_rev = 0;
    record->version = CHARGE_RECORD_VERSION;
    return save_config(charge_records_addr(profile), record, sizeof(*record));
}


static bool charge_records_write_unit(void) {
    charge_unit_record.legacy_rev = 0;
    charge_unit_record.version = CHARGE_RECORD_VERSION;
    return save_config(EEPROM_UNIT_RECORD_ADDR, &charge_unit_record, sizeof(charge_unit_record));
}


static void charge_records_to_stop(charge_record_stop_t *to, const charge_control_stop_t *from) {
    *to = (charge_record_stop_t) {from->speed_rps, from->unseen_gr, from->spread_gr, from->quiet_s, from->lag_s};
}


static void charge_records_from_stop(charge_control_stop_t *to, const charge_record_stop_t *from) {
    *to = (charge_control_stop_t) {from->speed_rps, from->unseen_gr, from->spread_gr, from->quiet_s, from->lag_s};
}


// The working form from the stored one, its scale's part the unit record's
static void charge_records_to_working(const charge_powder_record_t *r, charge_calibration_record_t *out) {
    memset(out, 0, sizeof(*out));
    charge_control_calibration_t *m = &out->measured;
    out->valid = r->held;
    m->digit_gr = charge_unit_record.digit_gr;
    m->interval_s = charge_unit_record.interval_s;
    m->coarse_gr_turn = r->coarse_gr_turn;
    m->fine_gr_turn = r->fine_gr_turn;
    m->coarse_share = r->coarse_share;
    m->watch_s = r->watch_s;
    m->finish = CHARGE_CONTROL_FINISH_GLIDE;
    m->coarse_stop_count = r->coarse_stops.count <= CHARGE_CONTROL_COARSE_STOPS_MAX ? r->coarse_stops.count : 0;
    for (uint8_t i = 0; i < m->coarse_stop_count; i++) {
        charge_records_from_stop(&m->coarse_stops[i], &r->coarse_stops.rows[i]);
    }
    m->fine_stop_count = r->fine_stops.count <= CHARGE_CONTROL_FINE_STOPS_MAX ? r->fine_stops.count : 0;
    for (uint8_t i = 0; i < m->fine_stop_count; i++) {
        charge_records_from_stop(&m->fine_stops[i], &r->fine_stops.rows[i]);
    }
    m->move_count = r->moves.count <= CHARGE_CONTROL_MOVES ? r->moves.count : 0;
    for (uint8_t i = 0; i < m->move_count; i++) {
        const charge_record_move_t *v = &r->moves.rows[i];
        m->moves[i] = (charge_control_move_t) {v->turns, v->mean_gr, v->most_gr, v->wait_s};
    }
    out->first_error_count = r->first_errors.count <= CHARGE_CALIBRATION_FIRST_ERRORS_MAX ? r->first_errors.count : 0;
    for (uint8_t i = 0; i < out->first_error_count; i++) {
        out->first_errors[i] = r->first_errors.rows[i].error_gr;
    }
}


// The stored form from the working one; its unit this unit
static void charge_records_from_working(const charge_calibration_record_t *w, uint8_t law, charge_powder_record_t *r) {
    const charge_control_calibration_t *m = &w->measured;
    memset(r, 0, sizeof(*r));
    r->held = w->valid;
    charge_records_this_unit(r->unit_id);
    r->coarse_gr_turn = m->coarse_gr_turn;
    r->fine_gr_turn = m->fine_gr_turn;
    r->coarse_share = m->coarse_share;
    r->watch_s = m->watch_s;
    r->law = law;
    r->coarse_stops.count = m->coarse_stop_count <= CHARGE_RECORD_COARSE_STOPS ? m->coarse_stop_count : 0;
    for (uint8_t i = 0; i < r->coarse_stops.count; i++) {
        charge_records_to_stop(&r->coarse_stops.rows[i], &m->coarse_stops[i]);
    }
    r->fine_stops.count = m->fine_stop_count <= CHARGE_RECORD_FINE_STOPS ? m->fine_stop_count : 0;
    for (uint8_t i = 0; i < r->fine_stops.count; i++) {
        charge_records_to_stop(&r->fine_stops.rows[i], &m->fine_stops[i]);
    }
    r->moves.count = m->move_count <= CHARGE_RECORD_MOVES ? m->move_count : 0;
    for (uint8_t i = 0; i < r->moves.count; i++) {
        const charge_control_move_t *v = &m->moves[i];
        r->moves.rows[i] = (charge_record_move_t) {v->turns, v->mean_gr, v->most_gr, v->wait_s};
    }
    r->first_errors.count = w->first_error_count <= CHARGE_RECORD_ERRORS ? w->first_error_count : 0;
    for (uint8_t i = 0; i < r->first_errors.count; i++) {
        r->first_errors.rows[i].error_gr = w->first_errors[i];
    }
}


charge_record_state_t charge_records_load(uint8_t profile, charge_calibration_record_t *out, uint8_t *law) {
    if (profile >= CHARGE_RECORD_PROFILES) {
        return CHARGE_RECORD_NONE;
    }
    charge_powder_record_t *r = pvPortMalloc(sizeof(*r));
    if (r == NULL) {
        return CHARGE_RECORD_DAMAGED;   // not read is not used; the console says why
    }
    charge_record_state_t state = charge_records_read(profile, r);
    if (state == CHARGE_RECORD_OK) {
        charge_records_to_working(r, out);
        if (law != NULL) {
            *law = r->law;
        }
    }
    vPortFree(r);
    return state;
}


bool charge_records_save(uint8_t profile, const charge_calibration_record_t *record, uint8_t law) {
    if (profile >= CHARGE_RECORD_PROFILES) {
        return false;
    }
    charge_powder_record_t *r = pvPortMalloc(sizeof(*r));
    if (r == NULL) {
        return false;
    }
    charge_records_from_working(record, law, r);
    bool ok = charge_records_write(profile, r);
    vPortFree(r);
    // The scale's part, the unit's: the calibration measures it with every powder
    if (ok && record->valid && (charge_unit_record.digit_gr != record->measured.digit_gr ||
                                charge_unit_record.interval_s != record->measured.interval_s || !charge_unit_record.held)) {
        charge_unit_record.held = 1;
        charge_records_this_unit(charge_unit_record.unit_id);
        charge_unit_record.digit_gr = record->measured.digit_gr;
        charge_unit_record.interval_s = record->measured.interval_s;
        ok = charge_records_write_unit();
    }
    charge_records_forget(profile);
    return ok;
}


bool charge_records_corrupt(uint8_t profile) {
    uint8_t byte;
    uint16_t addr = (uint16_t) (charge_records_addr(profile) + offsetof(charge_powder_record_t, coarse_gr_turn));
    if (profile >= CHARGE_RECORD_PROFILES || !eeprom_read(addr, &byte, 1)) {
        return false;
    }
    byte ^= 0x5a;
    charge_records_forget(profile);
    return eeprom_write(addr, &byte, 1);
}


const char *charge_records_state_words(charge_record_state_t state) {
    return state == CHARGE_RECORD_DAMAGED ? "Calibration damaged: calibrate again" :
           state == CHARGE_RECORD_NONE ? "Not calibrated" : "Calibrated";
}


// --- The migration from the form before gate R (v1: the controller's struct, stored whole)

void charge_records_init(void) {
    if (!config_read_stored(EEPROM_UNIT_RECORD_ADDR, &charge_unit_record, sizeof(charge_unit_record)) ||
        charge_unit_record.version != CHARGE_RECORD_VERSION) {
        memset(&charge_unit_record, 0, sizeof(charge_unit_record));
        charge_unit_record.scale_lag_s = CHARGE_RECORD_NOT_MEASURED;
        charge_unit_record.scale_rest_s = CHARGE_RECORD_NOT_MEASURED;
    }
    charge_powder_record_t *r = pvPortMalloc(sizeof(*r));
    charge_calibration_record_t *v1 = pvPortMalloc(sizeof(*v1));
    uint8_t migrated = 0;
    if (r != NULL && v1 != NULL) {
        for (uint8_t profile = 0; profile < CHARGE_RECORD_PROFILES; profile++) {
            uint16_t addr = charge_records_addr(profile);
            if (charge_records_read(profile, r) != CHARGE_RECORD_DAMAGED) {
                continue;
            }
            if (!config_read_stored(addr, v1, sizeof(*v1)) || !v1->valid) {
                continue;   // damaged, and no v1 either: left as it is, and a charge says so
            }
            // The scale's part from the first valid one; another that disagrees is said, never averaged
            if (!charge_unit_record.held) {
                charge_unit_record.held = 1;
                charge_records_this_unit(charge_unit_record.unit_id);
                charge_unit_record.digit_gr = v1->measured.digit_gr;
                charge_unit_record.interval_s = v1->measured.interval_s;
                charge_records_write_unit();
            }
            else if (charge_unit_record.digit_gr != v1->measured.digit_gr) {
                printf("Records: profile %u's calibration gives a digit of %g, the unit's record %g: the first kept\n",
                       profile, v1->measured.digit_gr, charge_unit_record.digit_gr);
            }
            charge_records_from_working(v1, 0, r);      // its own list, the glide's
            if (charge_records_write(profile, r)) {
                migrated++;
                printf("Records: profile %u's calibration made a v2 record (%u stops fine, %u coarse, %u carried)\n",
                       profile, r->fine_stops.count, r->coarse_stops.count, r->first_errors.count);
            }
        }
    }
    vPortFree(v1);
    vPortFree(r);
    // The PID law's old lists (21 K): their slot zeroed, read by nothing again
    if (!charge_records_blank(CHARGE_RECORD_LAW_STOPS_OLD_ADDR)) {
        uint8_t zeros[64] = {0};
        bool ok = true;
        for (uint16_t at = 0; at < CHARGE_RECORD_LAW_STOPS_OLD_SIZE; at += sizeof(zeros)) {
            ok = ok && eeprom_write((uint16_t) (CHARGE_RECORD_LAW_STOPS_OLD_ADDR + at), zeros, sizeof(zeros));
        }
        printf("Records: the PID law's old lists at 21 K %s\n", ok ? "zeroed" : "not zeroed: the EEPROM refused");
    }
    if (migrated) {
        printf("Records: %u calibrations moved to the records of gate R\n", migrated);
    }
    // The API's copy of one powder's record, taken once while the heap is whole: without it the records are not
    // served, and a charge still reads them
    charge_records_cache = pvPortMalloc(sizeof(*charge_records_cache));
    if (charge_records_cache == NULL || !charge_records_register()) {
        printf("Records: not served by the API: no memory or no room for their routes\n");
    }
}


// --- The API: two settings resources. A powder's record is stored at its save; the unit's, small enough for one
// write, at once

#define RECORD_LIST_ROWS(type) .rows_offset = offsetof(type, rows), .stride = sizeof(((type *) 0)->rows[0])
#define ROW_NUMBER(field_name, struct_type, member, unit_text, low, high, text) \
    {.name = field_name, .type = SETTING_NUMBER, SETTING_AT(struct_type, member), .unit = unit_text, .min = low, \
     .max = high, .about = text, .applies = "at_charge_start"}

// The bounds: a value outside is no measurement of its kind on any unit, not a limit of this one (numbers.md, B13)
static const setting_field_t stop_row[] = {
    ROW_NUMBER("speed_rps", charge_record_stop_t, speed_rps, "rev/s", 0.0f, 20.0f, "the speed the tube stopped from"),
    ROW_NUMBER("unseen_gr", charge_record_stop_t, unseen_gr, "gr", -1.0f, 100.0f, "what still arrived after its stop"),
    ROW_NUMBER("spread_gr", charge_record_stop_t, spread_gr, "gr", 0.0f, 20.0f, "how widely that varied (an sd)"),
    ROW_NUMBER("quiet_s", charge_record_stop_t, quiet_s, "s", 0.0f, 60.0f, "until the reading was still"),
    ROW_NUMBER("lag_s", charge_record_stop_t, lag_s, "s", -1.0f, 10.0f,
               "its powder's time from turned out to shown; -1 when not measured"),
};
static const setting_field_t move_row[] = {
    ROW_NUMBER("turns", charge_record_move_t, turns, "turns", 0.0f, 5.0f, "the move's turns"),
    ROW_NUMBER("mean_gr", charge_record_move_t, mean_gr, "gr", -1.0f, 10.0f, "what it released, on average"),
    ROW_NUMBER("most_gr", charge_record_move_t, most_gr, "gr", -1.0f, 20.0f, "nine in ten released at most this"),
    ROW_NUMBER("wait_s", charge_record_move_t, wait_s, "s", 0.0f, 60.0f, "the wait to see it"),
};
static const setting_field_t error_row[] = {
    ROW_NUMBER("error_gr", charge_record_error_t, error_gr, "gr", -5.0f, 5.0f,
               "a throw's first error at rest, against its aim: where the stop is placed from"),
};
static const setting_list_t coarse_list = {SETTING_FIELDS(stop_row), .rows_min = 0,
                                           .rows_max = CHARGE_RECORD_COARSE_STOPS, RECORD_LIST_ROWS(coarse_list_t)};
static const setting_list_t fine_list = {SETTING_FIELDS(stop_row), .rows_min = 0,
                                         .rows_max = CHARGE_RECORD_FINE_STOPS, RECORD_LIST_ROWS(fine_list_t)};
static const setting_list_t move_list = {SETTING_FIELDS(move_row), .rows_min = 0, .rows_max = CHARGE_RECORD_MOVES,
                                         RECORD_LIST_ROWS(move_list_t)};
static const setting_list_t error_list = {SETTING_FIELDS(error_row), .rows_min = 0, .rows_max = CHARGE_RECORD_ERRORS,
                                          RECORD_LIST_ROWS(error_list_t)};

static const setting_choice_t law_choices[] = {
    {.held = 0, .text = "glide", .label = "The glide's"},
    {.held = 1, .text = "pid", .label = "The PID law's"},
};

#define POWDER_FIELD(member) SETTING_AT(charge_powder_record_t, member)
static const setting_field_t powder_fields[] = {
    {.name = "unit_id", .label = "Unit", .type = SETTING_STRING, POWDER_FIELD(unit_id), .applies = "at_charge_start",
     .required = true,
     .about = "The unit it was measured on: every write names it, and one naming another is refused, 409 not_this_unit"},
    {.name = "held", .label = "Calibrated", .type = SETTING_BOOLEAN, POWDER_FIELD(held), .applies = "at_charge_start",
     .about = "A calibration is held, and a charge may use it once it holds what a throw needs"},
    {.name = "coarse_gr_turn", .label = "Coarse grains a turn", .type = SETTING_NUMBER, POWDER_FIELD(coarse_gr_turn),
     .unit = "gr", .min = 0.0f, .max = 200.0f, .applies = "at_charge_start", .about = "The coarse tube's powder a turn"},
    {.name = "fine_gr_turn", .label = "Fine grains a turn", .type = SETTING_NUMBER, POWDER_FIELD(fine_gr_turn),
     .unit = "gr", .min = 0.0f, .max = 50.0f, .applies = "at_charge_start", .about = "The fine tube's powder a turn"},
    {.name = "coarse_share", .label = "Coarse spread", .type = SETTING_NUMBER, POWDER_FIELD(coarse_share),
     .min = 0.0f, .max = 1.0f, .applies = "at_charge_start",
     .about = "How far the coarse tube's powder a turn varied from run to run, as a share"},
    {.name = "watch_s", .label = "Watch time", .type = SETTING_NUMBER, POWDER_FIELD(watch_s), .unit = "s",
     .min = 0.0f, .max = 120.0f, .applies = "at_charge_start", .about = "How long its stops and moves were watched"},
    {.name = "law", .label = "Carried by", .type = SETTING_CHOICE, POWDER_FIELD(law), SETTING_CHOICES(law_choices),
     .applies = "at_charge_start",
     .about = "The law that learnt the carried stops: a charge under the other takes its first carry whole"},
    {.name = "coarse_stops", .label = "Coarse stops", .type = SETTING_LIST, POWDER_FIELD(coarse_stops),
     .list = &coarse_list, .applies = "at_charge_start", .about = "The coarse tube's stops, by speed, slowest first"},
    {.name = "fine_stops", .label = "Fine stops", .type = SETTING_LIST, POWDER_FIELD(fine_stops), .list = &fine_list,
     .applies = "at_charge_start", .about = "The fine tube's stops, by speed, slowest first"},
    {.name = "moves", .label = "Small moves", .type = SETTING_LIST, POWDER_FIELD(moves), .list = &move_list,
     .applies = "at_charge_start", .about = "The fine tube's small moves, smallest first"},
    {.name = "first_errors", .label = "Carried stops", .type = SETTING_LIST, POWDER_FIELD(first_errors),
     .list = &error_list, .applies = "at_charge_start", .about = "The newest throws' first errors, oldest first"},
};

#define UNIT_FIELD(member) SETTING_AT(charge_unit_record_t, member)
static const setting_field_t unit_fields[] = {
    {.name = "unit_id", .label = "Unit", .type = SETTING_STRING, UNIT_FIELD(unit_id), .applies = "at_charge_start",
     .required = true,
     .about = "The unit it was measured on: every write names it, and one naming another is refused, 409 not_this_unit"},
    {.name = "held", .label = "Measured", .type = SETTING_BOOLEAN, UNIT_FIELD(held), .applies = "at_charge_start",
     .about = "The scale's part has been measured"},
    {.name = "digit_gr", .label = "Digit", .type = SETTING_NUMBER, UNIT_FIELD(digit_gr), .unit = "gr", .min = 0.0f,
     .max = 1.0f, .applies = "at_charge_start", .about = "The scale's smallest step"},
    {.name = "interval_s", .label = "Interval", .type = SETTING_NUMBER, UNIT_FIELD(interval_s), .unit = "s",
     .min = 0.0f, .max = 2.0f, .applies = "never",
     .about = "The scale's usual time between readings, as last measured: kept for the simulation; the unit reads its "
              "live rhythm"},
    {.name = "scale_lag_s", .label = "Scale lag", .type = SETTING_NUMBER, UNIT_FIELD(scale_lag_s), .unit = "s",
     .min = -1.0f, .max = 10.0f, .applies = "at_charge_start",
     .about = "How long the scale takes to show a weight: -1 until a calibration measures it"},
    {.name = "scale_rest_s", .label = "Scale rest", .type = SETTING_NUMBER, UNIT_FIELD(scale_rest_s), .unit = "s",
     .min = -1.0f, .max = 30.0f, .applies = "at_charge_start",
     .about = "How long the scale takes to settle: -1 until a calibration measures it"},
};


// What is stored of a powder, as a read gives it: none stored, or one that fails its check, reads empty
static void powder_stored(int32_t id, charge_powder_record_t *out) {
    if (charge_records_read((uint8_t) id, out) != CHARGE_RECORD_OK) {
        memset(out, 0, sizeof(*out));
        charge_records_this_unit(out->unit_id);
    }
}


// For a write alone (the settings' reads take a copy: powder_read). Another powder's write is refused while one is
// staged (powder_refuse), so the copy moves to another powder only from a revert's, which drops it
static void *powder_live(int32_t id) {
    if (charge_records_cache_id != (int8_t) id) {
        powder_stored(id, charge_records_cache);
        charge_records_cache_id = (int8_t) id;
        charge_records_staged = false;
    }
    return charge_records_cache;
}


// A read's copy: the write staged for this powder, or what is stored
static void powder_read(int32_t id, bool stored, void *out) {
    if (!stored && charge_records_staged && charge_records_cache_id == (int8_t) id) {
        memcpy(out, charge_records_cache, sizeof(*charge_records_cache));
    }
    else {
        powder_stored(id, out);
    }
}


// One powder's write staged at a time: a write of another names it, and how to clear it
static const char *powder_refuse(int32_t id) {
    static char why[160];
    if (!charge_records_staged || charge_records_cache_id == (int8_t) id) {
        return NULL;
    }
    snprintf(why, sizeof(why), "powder_records/%d has a write not saved: save it (POST %s) or revert it (POST %s) "
             "first", charge_records_cache_id, RECORDS_POWDER_SAVE, RECORDS_POWDER_REVERT);
    return why;
}


static const char *records_foreign(const char *unit_id) {
    char mine[CHARGE_RECORD_UNIT_ID_ROOM];
    charge_records_this_unit(mine);
    return strncmp(unit_id, mine, CHARGE_RECORD_UNIT_ID_ROOM) == 0 ? NULL :
           "it was measured on another unit: a unit's own data is never applied to another";
}


static const char *powder_foreign(const void *proposed, int32_t id) {
    (void) id;
    return records_foreign(((const charge_powder_record_t *) proposed)->unit_id);
}


static const char *unit_foreign(const void *proposed, int32_t id) {
    (void) id;
    return records_foreign(((const charge_unit_record_t *) proposed)->unit_id);
}


// Each list's speeds rising: a ladder, slowest first
static const char *powder_check(const void *proposed, int32_t id, const char **field) {
    (void) id;
    const charge_powder_record_t *r = proposed;
    for (uint8_t i = 1; i < r->coarse_stops.count && i < CHARGE_RECORD_COARSE_STOPS; i++) {
        if (r->coarse_stops.rows[i].speed_rps <= r->coarse_stops.rows[i - 1].speed_rps) {
            *field = "coarse_stops";
            return "the coarse stops' speeds rise, slowest first";
        }
    }
    for (uint8_t i = 1; i < r->fine_stops.count && i < CHARGE_RECORD_FINE_STOPS; i++) {
        if (r->fine_stops.rows[i].speed_rps <= r->fine_stops.rows[i - 1].speed_rps) {
            *field = "fine_stops";
            return "the fine stops' speeds rise, slowest first";
        }
    }
    return NULL;
}


// Staged until its save: what differs from what is stored (a write back of what is stored stages nothing)
static void powder_apply(const void *before, int32_t id) {
    (void) before;
    charge_powder_record_t *stored = pvPortMalloc(sizeof(*stored));
    if (stored == NULL) {
        charge_records_staged = true;
        return;
    }
    powder_stored(id, stored);
    charge_records_staged = memcmp(stored, charge_records_cache, sizeof(*stored)) != 0;
    vPortFree(stored);
}


static bool powder_is_saved(void) {
    return !charge_records_staged;
}


static bool powder_save(void) {
    if (!charge_records_staged || charge_records_cache_id < 0) {
        return true;
    }
    if (!charge_records_write((uint8_t) charge_records_cache_id, charge_records_cache)) {
        return false;
    }
    charge_records_staged = false;
    return true;
}


static void *unit_live(int32_t id) {
    (void) id;
    return &charge_unit_record;
}


static void unit_apply(const void *before, int32_t id) {
    (void) before;
    (void) id;
    charge_records_write_unit();
}


static bool records_saved(void) {
    return true;        // the unit's record: every write is stored as it is made
}


static bool records_save(void) {
    return true;
}


static const settings_resource_t powder_settings = {
    .path = RECORDS_POWDER_PATH,
    .save_path = RECORDS_POWDER_SAVE,
    .revert_path = RECORDS_POWDER_REVERT,
    .addr = EEPROM_CHARGE_CALIBRATION_BASE_ADDR,
    .stride = CHARGE_RECORD_SLOT_SIZE,
    .label = "This unit's powders",
    .about = "What this unit measured of each powder profile's powder: this unit's own, kept apart from the profile. "
             "A write takes effect when it is saved: a charge reads only what is stored. One powder's write is held "
             "at a time, until its save or its revert. Refused while a job runs or when it names another unit",
    SETTING_FIELDS(powder_fields),
    .refused_while = SETTINGS_REFUSED_A_JOB_RUNS,
    .size = sizeof(charge_powder_record_t),
    .id_count = CHARGE_RECORD_PROFILES,
    .live = powder_live,
    .read = powder_read,
    .check = powder_check,
    .apply = powder_apply,
    .refuse = powder_refuse,
    .foreign = powder_foreign,
    .save = powder_save,
    .is_saved = powder_is_saved,
};

static const settings_resource_t unit_settings = {
    .path = "/api/v1/settings/unit_record",
    .save_path = "/api/v1/settings/unit_record/save",
    .revert_path = "/api/v1/settings/unit_record/revert",
    .addr = EEPROM_UNIT_RECORD_ADDR,
    .label = "This unit's scale",
    .about = "What this unit measured of its scale: this unit's own. A write is stored at once, and refused while a "
             "job runs or when it names another unit",
    SETTING_FIELDS(unit_fields),
    .refused_while = SETTINGS_REFUSED_A_JOB_RUNS,
    .size = sizeof(charge_unit_record_t),
    .live = unit_live,
    .apply = unit_apply,
    .foreign = unit_foreign,
    .save = records_save,
    .is_saved = records_saved,
};


bool charge_records_register(void) {
    return settings_register(&unit_settings) && settings_register(&powder_settings);
}
