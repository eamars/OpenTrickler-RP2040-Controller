#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charge_calibration.h"
#include "common.h"
#include "eeprom.h"
#include "profile.h"
#include "scale.h"

#define CHARGE_CALIBRATION_SLOT_SIZE 1024

static charge_control_constants_t charge_constants;
static bool charge_constants_set = false;

// The console's calibration being written, before `ccal save`
static charge_calibration_record_t charge_calibration_staged;
// A throw's read of its slot: the job task's alone
static charge_calibration_record_t charge_calibration_read;

_Static_assert(sizeof(charge_calibration_record_t) + sizeof(uint32_t) <= CHARGE_CALIBRATION_SLOT_SIZE,
               "a calibration and its CRC fit their slot");


const charge_control_constants_t *charge_calibration_constants(void) {
    if (!charge_constants_set) {
        charge_control_default_constants(&charge_constants);
        charge_constants_set = true;
    }
    return &charge_constants;
}


static uint16_t charge_calibration_addr(uint8_t profile) {
    return (uint16_t) (EEPROM_CHARGE_CALIBRATION_BASE_ADDR + profile * CHARGE_CALIBRATION_SLOT_SIZE);
}


// The rules made from a record's measured part, its finish and band, then its stop placed by its rehearsals
static void charge_calibration_make(const charge_calibration_record_t *record, float band_gr,
                                    charge_control_calibration_t *out) {
    *out = record->measured;
    charge_control_finish_t finish = (charge_control_finish_t) record->measured.finish;
    if (band_gr > 0.0f) {
        finish = CHARGE_CONTROL_FINISH_GLIDE;       // the modes are the glide's (ADR-009, 2026-11-22)
    }
    else {
        band_gr = record->measured.band_gr;
    }
    charge_control_set_rules(out, finish, band_gr, charge_calibration_constants());
    if (record->first_error_count > 0) {
        charge_control_place_stop(out, record->first_errors, record->first_error_count);
    }
}


// What a calibration needs before a throw may use it; NULL when it has it
static const char *charge_calibration_lacks(const charge_calibration_record_t *record) {
    const charge_control_calibration_t *cal = &record->measured;
    if (!(cal->digit_gr > 0.0f) || !(cal->interval_s > 0.0f) || !(cal->coarse_gr_turn > 0.0f) ||
        !(cal->fine_gr_turn > 0.0f)) {
        return "head: digit, interval and each tube's grains a turn";
    }
    if (cal->coarse_stop_count < 1 || cal->fine_stop_count < 3) {
        return "stops: a coarse one at least, three fine";
    }
    if (cal->move_count != CHARGE_CONTROL_MOVES) {
        return "moves: three";
    }
    for (uint8_t i = 0; i < cal->fine_stop_count; i++) {
        if (!(cal->fine_stops[i].speed_rps > 0.0f) || (i > 0 && cal->fine_stops[i].speed_rps <= cal->fine_stops[i - 1].speed_rps)) {
            return "fine stops: each faster than the last";
        }
    }
    for (uint8_t i = 0; i < cal->coarse_stop_count; i++) {
        if (!(cal->coarse_stops[i].speed_rps > 0.0f) ||
            (i > 0 && cal->coarse_stops[i].speed_rps <= cal->coarse_stops[i - 1].speed_rps)) {
            return "coarse stops: each faster than the last";
        }
    }
    if (cal->finish > CHARGE_CONTROL_FINISH_JOG || !(cal->band_gr > 0.0f)) {
        return "finish: glide or jog, and a band";
    }
    return NULL;
}


/*
    The simulated scale's own calibration (ADR-009, gate 2): its tubes pour their made-up grains a turn at once and
    evenly, and it reads them at its interval, so a stop at any speed leaves half an interval of flow unseen, and a
    small move releases its turns' grains. Made from the scale's own constants, not measured; a charge on the
    simulated scale takes it whatever the profile holds, and nothing stored is read or written. The glide at 0.02 gr
*/
static void charge_calibration_simulated(charge_calibration_record_t *record) {
    static const float coarse_speeds[] = {0.3f, 1.0f, 2.0f};
    static const float fine_speeds[] = {0.025f, 0.05f, 0.1f, 0.2f, 0.5f};
    static const float move_digits[] = {0.7f, 2.5f, 6.0f};
    charge_control_calibration_t *cal = &record->measured;
    memset(record, 0, sizeof(*record));
    float interval_s = 1.0f / simulated_scale_get_rate();
    float lag_s = interval_s / 2.0f;
    cal->digit_gr = SIMULATED_SCALE_DIGIT_GR;
    cal->interval_s = interval_s;
    cal->coarse_gr_turn = SIMULATED_SCALE_COARSE_GR_TURN;
    cal->fine_gr_turn = SIMULATED_SCALE_FINE_GR_TURN;
    cal->coarse_stop_count = sizeof(coarse_speeds) / sizeof(coarse_speeds[0]);
    cal->fine_stop_count = sizeof(fine_speeds) / sizeof(fine_speeds[0]);
    cal->move_count = CHARGE_CONTROL_MOVES;
    for (uint8_t i = 0; i < cal->coarse_stop_count; i++) {
        cal->coarse_stops[i] = (charge_control_stop_t) {coarse_speeds[i], coarse_speeds[i] * cal->coarse_gr_turn * lag_s,
                                                        cal->digit_gr, lag_s + interval_s, lag_s};
    }
    for (uint8_t i = 0; i < cal->fine_stop_count; i++) {
        cal->fine_stops[i] = (charge_control_stop_t) {fine_speeds[i], fine_speeds[i] * cal->fine_gr_turn * lag_s,
                                                      cal->digit_gr / 2.0f, lag_s + interval_s, lag_s};
    }
    for (uint8_t i = 0; i < CHARGE_CONTROL_MOVES; i++) {
        float grains = move_digits[i] * cal->digit_gr;
        cal->moves[i] = (charge_control_move_t) {grains / cal->fine_gr_turn, grains, grains + cal->digit_gr,
                                                 lag_s + 2.0f * interval_s};
    }
    cal->watch_s = 1.0f;
    cal->coarse_share = 0.01f;
    cal->finish = CHARGE_CONTROL_FINISH_GLIDE;
    cal->band_gr = 0.02f;
    record->valid = 1;
}


static bool charge_calibration_simulated_scale(void) {
    return scale_get_running_driver() == SCALE_DRIVER_SIMULATED;
}


bool charge_calibration_held(uint8_t profile) {
    if (charge_calibration_simulated_scale()) {
        return profile < MAX_PROFILE_CNT;
    }
    return profile < MAX_PROFILE_CNT &&
           config_read_stored(charge_calibration_addr(profile), &charge_calibration_read,
                              sizeof(charge_calibration_read)) &&
           charge_calibration_read.valid && charge_calibration_lacks(&charge_calibration_read) == NULL;
}


bool charge_calibration_get(uint8_t profile, float band_gr, charge_control_calibration_t *out) {
    if (charge_calibration_simulated_scale() && profile < MAX_PROFILE_CNT) {
        charge_calibration_simulated(&charge_calibration_read);
        charge_calibration_make(&charge_calibration_read, band_gr, out);
        return true;
    }
    if (!charge_calibration_held(profile)) {
        return false;
    }
    charge_calibration_make(&charge_calibration_read, band_gr, out);
    return true;
}


// Up to `most` numbers from text, separated by spaces; how many were read, or -1 for one that is not a number
static int charge_calibration_numbers(const char *text, float *values, int most) {
    int count = 0;
    while (*text != '\0') {
        while (*text == ' ') {
            text++;
        }
        if (*text == '\0') {
            break;
        }
        if (count == most) {
            return -1;
        }
        char *end;
        float value = strtof(text, &end);
        if (end == text || !isfinite(value)) {
            return -1;
        }
        values[count++] = value;
        text = end;
    }
    return count;
}


static void charge_calibration_show(const charge_calibration_record_t *record, remote_control_write_fn write,
                                    void *write_ctx) {
    char line[200];
    const charge_control_calibration_t *m = &record->measured;
    snprintf(line, sizeof(line), "@cal head digit=%g interval=%g coarse_gr_turn=%g fine_gr_turn=%g watch=%g "
             "coarse_share=%g", m->digit_gr, m->interval_s, m->coarse_gr_turn, m->fine_gr_turn, m->watch_s,
             m->coarse_share);
    write(line, write_ctx);
    for (uint8_t i = 0; i < m->coarse_stop_count; i++) {
        const charge_control_stop_t *s = &m->coarse_stops[i];
        snprintf(line, sizeof(line), "@cal stop coarse %u speed=%g unseen=%g spread=%g quiet=%g lag=%g", i,
                 s->speed_rps, s->unseen_gr, s->spread_gr, s->quiet_s, s->lag_s);
        write(line, write_ctx);
    }
    for (uint8_t i = 0; i < m->fine_stop_count; i++) {
        const charge_control_stop_t *s = &m->fine_stops[i];
        snprintf(line, sizeof(line), "@cal stop fine %u speed=%g unseen=%g spread=%g quiet=%g lag=%g", i,
                 s->speed_rps, s->unseen_gr, s->spread_gr, s->quiet_s, s->lag_s);
        write(line, write_ctx);
    }
    for (uint8_t i = 0; i < m->move_count; i++) {
        const charge_control_move_t *v = &m->moves[i];
        snprintf(line, sizeof(line), "@cal move %u turns=%g mean=%g most=%g wait=%g", i, v->turns, v->mean_gr,
                 v->most_gr, v->wait_s);
        write(line, write_ctx);
    }
    size_t len = (size_t) snprintf(line, sizeof(line), "@cal errors");
    for (uint8_t i = 0; i < record->first_error_count && len < sizeof(line); i++) {
        len += (size_t) snprintf(line + len, sizeof(line) - len, " %g", record->first_errors[i]);
    }
    write(line, write_ctx);
    const char *lacks = charge_calibration_lacks(record);
    if (lacks != NULL) {
        snprintf(line, sizeof(line), "@cal lacks %s", lacks);
        write(line, write_ctx);
        return;
    }
    charge_control_calibration_t made;
    charge_calibration_make(record, 0.0f, &made);
    snprintf(line, sizeof(line), "@cal rules finish=%s band=%g floor=%g stop_room=%g aim=%g glide_room=%g glide=%g",
             made.finish == CHARGE_CONTROL_FINISH_JOG ? "jog" : "glide", made.band_gr, made.floor_rps,
             made.stop_room_gr, made.aim_gr, made.glide_room_gr, made.glide_s);
    write(line, write_ctx);
    snprintf(line, sizeof(line), "@cal rules coarse_room=%g coarse_glide=%g rest=%g", made.coarse_room_gr,
             made.coarse_glide_s, made.rest_s);
    write(line, write_ctx);
}


/*
    `ccal`: the calibration being written, shown
    `ccal clear`
    `ccal head <digit> <interval s> <coarse gr/turn> <fine gr/turn> <watch s> <coarse share>`
    `ccal stop <coarse|fine> <i> <speed rev/s> <unseen gr> <spread gr> <quiet s> <lag s, below 0 when none>`
    `ccal move <i> <turns> <mean gr> <most gr> <wait s>`
    `ccal errors <gr> ...`: the rehearsals' first errors, added to those given
    `ccal finish <glide|jog> <band gr>`
    `ccal save [<profile>]`, `ccal load [<profile>]`: the slot written, or read into the one being written; the
    profile chosen when none is given
*/
void charge_calibration_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    charge_calibration_record_t *record = &charge_calibration_staged;
    charge_control_calibration_t *m = &record->measured;
    float v[CHARGE_CALIBRATION_FIRST_ERRORS_MAX];

    if (*args == '\0') {
        charge_calibration_show(record, write, write_ctx);
        snprintf(reply, reply_size, "@ok ccal");
        return;
    }
    if (strcmp(args, "clear") == 0) {
        memset(record, 0, sizeof(*record));
        record->measured.band_gr = 0.02f;
        snprintf(reply, reply_size, "@ok ccal clear");
        return;
    }
    if (strncmp(args, "head ", 5) == 0) {
        if (charge_calibration_numbers(args + 5, v, 6) != 6) {
            snprintf(reply, reply_size, "@err usage: ccal head <digit> <interval> <coarse gr/turn> <fine gr/turn> "
                     "<watch> <coarse share>");
            return;
        }
        m->digit_gr = v[0];
        m->interval_s = v[1];
        m->coarse_gr_turn = v[2];
        m->fine_gr_turn = v[3];
        m->watch_s = v[4];
        m->coarse_share = v[5];
        snprintf(reply, reply_size, "@ok ccal head");
        return;
    }
    if (strncmp(args, "stop ", 5) == 0) {
        const char *rest = args + 5;
        bool coarse = strncmp(rest, "coarse ", 7) == 0;
        bool fine = strncmp(rest, "fine ", 5) == 0;
        if ((!coarse && !fine) || charge_calibration_numbers(rest + (coarse ? 7 : 5), v, 6) != 6 || v[0] < 0.0f ||
            v[0] >= (coarse ? CHARGE_CONTROL_COARSE_STOPS_MAX : CHARGE_CONTROL_FINE_STOPS_MAX) || v[0] != floorf(v[0])) {
            snprintf(reply, reply_size, "@err usage: ccal stop <coarse|fine> <i> <speed> <unseen> <spread> <quiet> "
                     "<lag>");
            return;
        }
        uint8_t i = (uint8_t) v[0];
        charge_control_stop_t *stop = coarse ? &m->coarse_stops[i] : &m->fine_stops[i];
        stop->speed_rps = v[1];
        stop->unseen_gr = v[2];
        stop->spread_gr = v[3];
        stop->quiet_s = v[4];
        stop->lag_s = v[5];
        uint8_t *count = coarse ? &m->coarse_stop_count : &m->fine_stop_count;
        if (*count < i + 1) {
            *count = i + 1;
        }
        snprintf(reply, reply_size, "@ok ccal stop");
        return;
    }
    if (strncmp(args, "move ", 5) == 0) {
        if (charge_calibration_numbers(args + 5, v, 5) != 5 || v[0] < 0.0f || v[0] >= CHARGE_CONTROL_MOVES ||
            v[0] != floorf(v[0])) {
            snprintf(reply, reply_size, "@err usage: ccal move <i> <turns> <mean> <most> <wait>");
            return;
        }
        uint8_t i = (uint8_t) v[0];
        m->moves[i] = (charge_control_move_t) {.turns = v[1], .mean_gr = v[2], .most_gr = v[3], .wait_s = v[4]};
        if (m->move_count < i + 1) {
            m->move_count = i + 1;
        }
        snprintf(reply, reply_size, "@ok ccal move");
        return;
    }
    if (strncmp(args, "errors ", 7) == 0) {
        int room = CHARGE_CALIBRATION_FIRST_ERRORS_MAX - record->first_error_count;
        int count = charge_calibration_numbers(args + 7, v, room);
        if (count < 0) {
            snprintf(reply, reply_size, "@err numbers, %d more at most", room);
            return;
        }
        memcpy(&record->first_errors[record->first_error_count], v, (size_t) count * sizeof(float));
        record->first_error_count += (uint8_t) count;
        snprintf(reply, reply_size, "@ok ccal errors count=%u", record->first_error_count);
        return;
    }
    if (strncmp(args, "finish ", 7) == 0) {
        const char *rest = args + 7;
        bool jog = strncmp(rest, "jog ", 4) == 0;
        bool glide = strncmp(rest, "glide ", 6) == 0;
        if ((!jog && !glide) || charge_calibration_numbers(rest + (jog ? 4 : 6), v, 1) != 1 || !(v[0] > 0.0f)) {
            snprintf(reply, reply_size, "@err usage: ccal finish <glide|jog> <band>");
            return;
        }
        m->finish = jog ? CHARGE_CONTROL_FINISH_JOG : CHARGE_CONTROL_FINISH_GLIDE;
        m->band_gr = v[0];
        snprintf(reply, reply_size, "@ok ccal finish");
        return;
    }
    bool save = strncmp(args, "save", 4) == 0 && (args[4] == ' ' || args[4] == '\0');
    bool load = strncmp(args, "load", 4) == 0 && (args[4] == ' ' || args[4] == '\0');
    if (save || load) {
        // The profile given, or the one chosen
        char *end = (char *) args + 4;
        long profile = (long) profile_get_selected_idx();
        if (args[4] == ' ') {
            profile = strtol(args + 5, &end, 10);
        }
        if ((args[4] == ' ' && end == args + 5) || *end != '\0' || profile < 0 || profile >= MAX_PROFILE_CNT) {
            snprintf(reply, reply_size, "@err usage: ccal %s <profile 0..%d>", save ? "save" : "load",
                     MAX_PROFILE_CNT - 1);
            return;
        }
        uint16_t addr = charge_calibration_addr((uint8_t) profile);
        if (load) {
            if (!config_read_stored(addr, record, sizeof(*record)) || !record->valid) {
                memset(record, 0, sizeof(*record));
                snprintf(reply, reply_size, "@err profile %ld holds no calibration", profile);
                return;
            }
            charge_calibration_show(record, write, write_ctx);
            snprintf(reply, reply_size, "@ok ccal load %ld", profile);
            return;
        }
        const char *lacks = charge_calibration_lacks(record);
        if (lacks != NULL) {
            snprintf(reply, reply_size, "@err the calibration lacks %s", lacks);
            return;
        }
        record->legacy_rev = 0;
        record->valid = 1;
        if (!save_config(addr, record, sizeof(*record))) {
            snprintf(reply, reply_size, "@err the slot did not write");
            return;
        }
        charge_calibration_show(record, write, write_ctx);
        snprintf(reply, reply_size, "@ok ccal save %ld", profile);
        return;
    }
    snprintf(reply, reply_size, "@err usage: ccal [clear|head|stop|move|errors|finish|save <p>|load <p>]");
}


typedef struct {
    const char *name;
    size_t offset;
    bool is_count;
} charge_constant_t;

#define CHARGE_CONSTANT(member) {#member, offsetof(charge_control_constants_t, member), false}

static const charge_constant_t charge_constants_table[] = {
    CHARGE_CONSTANT(move_rps), CHARGE_CONSTANT(parcel_s), CHARGE_CONSTANT(fine_top_rps),
    CHARGE_CONSTANT(coarse_top_rps), CHARGE_CONSTANT(coarse_floor_rps), CHARGE_CONSTANT(dribble_digits_s),
    CHARGE_CONSTANT(coarse_spreads), CHARGE_CONSTANT(creep_band_share), CHARGE_CONSTANT(creep_most_s),
    CHARGE_CONSTANT(learn_share), CHARGE_CONSTANT(learn_most_digits), CHARGE_CONSTANT(learn_dead_digits), CHARGE_CONSTANT(learn_fine_prior),
    CHARGE_CONSTANT(learn_fine_most), CHARGE_CONSTANT(learn_fine_split),
    CHARGE_CONSTANT(still_least_s),
    {"most_moves", offsetof(charge_control_constants_t, most_moves), true},
    CHARGE_CONSTANT(throw_most_s), CHARGE_CONSTANT(fine_most_turns), CHARGE_CONSTANT(over_most_gr),
};

#define CHARGE_CONSTANTS_COUNT (sizeof(charge_constants_table) / sizeof(charge_constants_table[0]))


/*
    `ccon`: the frame's constants, a line each; `ccon <name>=<value> ...` sets them for the throws from the next
    on, until a restart; `ccon defaults` puts the research's back. Never stored: a value the bench settles on is
    made the default in charge_control.c
*/
void charge_constants_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                              void *write_ctx) {
    charge_control_constants_t *k = (charge_control_constants_t *) charge_calibration_constants();
    char line[96];

    if (strcmp(args, "defaults") == 0) {
        charge_control_default_constants(k);
        snprintf(reply, reply_size, "@ok ccon defaults");
        return;
    }
    // Every pair checked before any is written
    charge_control_constants_t next = *k;
    const char *at = args;
    while (*at != '\0') {
        while (*at == ' ') {
            at++;
        }
        if (*at == '\0') {
            break;
        }
        size_t name_len = strcspn(at, "=");
        const charge_constant_t *found = NULL;
        for (size_t i = 0; i < CHARGE_CONSTANTS_COUNT; i++) {
            if (strlen(charge_constants_table[i].name) == name_len &&
                strncmp(charge_constants_table[i].name, at, name_len) == 0) {
                found = &charge_constants_table[i];
            }
        }
        char *end;
        float value = at[name_len] == '=' ? strtof(at + name_len + 1, &end) : NAN;
        if (found == NULL || !isfinite(value) || value < 0.0f || (*end != ' ' && *end != '\0')) {
            snprintf(reply, reply_size, "@err usage: ccon [defaults | <name>=<value> ...]");
            return;
        }
        if (found->is_count) {
            *((uint8_t *) &next + found->offset) = (uint8_t) value;
        }
        else {
            *(float *) ((uint8_t *) &next + found->offset) = value;
        }
        at = end;
    }
    *k = next;
    for (size_t i = 0; i < CHARGE_CONSTANTS_COUNT; i++) {
        const charge_constant_t *c = &charge_constants_table[i];
        if (c->is_count) {
            snprintf(line, sizeof(line), "@ccon %s=%u", c->name, *((const uint8_t *) k + c->offset));
        }
        else {
            snprintf(line, sizeof(line), "@ccon %s=%g", c->name, *(const float *) ((const uint8_t *) k + c->offset));
        }
        write(line, write_ctx);
    }
    snprintf(reply, reply_size, "@ok ccon");
}
