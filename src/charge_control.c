#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "charge_control.h"
#include "charge_rules.h"

typedef charge_control_real_t real_t;
// A constant in the controller's own precision, folded by the compiler: no double arithmetic on the unit
#define REAL(x) ((real_t) (x))
#ifdef CHARGE_CONTROL_DOUBLE
#define ABS_R fabs
#define SQRT_R sqrt
#else
#define ABS_R fabsf
#define SQRT_R sqrtf
#endif

/*
    A port of tools/charge_sim/staged.py (ADR-009), kept in its order so the two can be read side by side. Where this
    differs, it says so:
    - Times are seconds since the throw's start, so a real_t keeps a millisecond however long the unit has run.
    - Given the unit's count of the turns, a parcel is still made every parcel_s, on the tick, its speed the turns
      over that stretch; staged.py made one at each count, which a PC gave every 40 to 80 ms.
    - The limits (a throw too long, too many fine turns, a reading far over) are the firmware's own.
*/

#define COARSE 0
#define FINE 1
// A weight at rest lies on the scale's digits, and a reading less a large base is off them in real_t by more than a
// millionth (10.02 over a cup of 60 gr: 0.0200043): the band's edge is a quarter digit past the band
#define BAND_EDGE_DIGITS REAL(CHARGE_RULE_BAND_EDGE_DIGITS)
#define LEARN_FINE_LEAST_TURNS REAL(CHARGE_RULE_LEARN_FINE_LEAST_TURNS)    // a stretch shorter than this teaches nothing
#define LEARN_FINE_LEAST_STREAM_TURNS REAL(CHARGE_RULE_LEARN_FINE_LEAST_STREAM_TURNS)
// Two times this close are the same time: a tie on the millisecond's grid falls the same way in real_t as in the
// reference's double (staged.py's TIE_S)
#define TIE_S REAL(CHARGE_RULE_TIE_S)


void charge_control_default_constants(charge_control_constants_t *k) {
    k->move_rps = REAL(CHARGE_RULE_MOVE_RPS);
    k->parcel_s = REAL(CHARGE_RULE_PARCEL_S);
    k->fine_top_rps = REAL(CHARGE_RULE_FINE_TOP_RPS);
    k->coarse_top_rps = REAL(CHARGE_RULE_COARSE_TOP_RPS);
    k->coarse_floor_rps = REAL(CHARGE_RULE_COARSE_FLOOR_RPS);
    k->dribble_digits_s = REAL(CHARGE_RULE_DRIBBLE_DIGITS_S);
    k->coarse_spreads = REAL(CHARGE_RULE_COARSE_SPREADS);
    k->creep_band_share = REAL(CHARGE_RULE_CREEP_BAND_SHARE);
    k->creep_most_s = REAL(CHARGE_RULE_CREEP_MOST_S);
    k->learn_share = REAL(CHARGE_RULE_LEARN_SHARE);
    k->learn_most_digits = REAL(CHARGE_RULE_LEARN_MOST_DIGITS);
    k->learn_dead_digits = REAL(CHARGE_RULE_LEARN_DEAD_DIGITS);
    k->learn_fine_prior = REAL(CHARGE_RULE_LEARN_FINE_PRIOR);
    k->learn_fine_most = REAL(CHARGE_RULE_LEARN_FINE_MOST);
    k->learn_fine_split = REAL(CHARGE_RULE_LEARN_FINE_SPLIT);
    k->still_least_s = REAL(CHARGE_RULE_STILL_LEAST_S);
    k->most_moves = CHARGE_RULE_MOST_MOVES;
    k->throw_most_s = REAL(CHARGE_RULE_THROW_MOST_S);
    k->fine_most_turns = REAL(CHARGE_RULE_FINE_MOST_TURNS);
    k->over_tolerances = REAL(CHARGE_RULE_OVER_TOLERANCES);
    k->speed_law = CHARGE_RULE_SPEED_LAW;
    k->pid_envelope = CHARGE_RULE_PID_ENVELOPE;
}


static real_t max_f(real_t a, real_t b) {
    return a > b ? a : b;
}


static real_t min_f(real_t a, real_t b) {
    return a < b ? a : b;
}


// The lag at a stop's speed: measured over its runs, or what was unseen at the stop over its flow
static real_t stop_lag_s(const charge_control_stop_t *stop, real_t gr_turn) {
    if (stop->lag_s >= REAL(0.0)) {
        return stop->lag_s;
    }
    return max_f(REAL(0.0), stop->unseen_gr) / (gr_turn * stop->speed_rps);
}


typedef enum {
    STOP_LAG,
    STOP_SPREAD,
} stop_field_t;


static real_t stop_value(const charge_control_stop_t *stop, stop_field_t field, real_t gr_turn) {
    return field == STOP_LAG ? stop_lag_s(stop, gr_turn) : stop->spread_gr;
}


// A measured table read at a speed: straight between two stops, the end stop's value outside them
static real_t stop_table(const charge_control_stop_t *stops, uint8_t count, stop_field_t field, real_t gr_turn,
                        real_t speed) {
    if (speed <= stops[0].speed_rps) {
        return stop_value(&stops[0], field, gr_turn);
    }
    for (uint8_t i = 1; i < count; i++) {
        if (speed <= stops[i].speed_rps) {
            real_t x0 = stops[i - 1].speed_rps;
            real_t y0 = stop_value(&stops[i - 1], field, gr_turn);
            real_t y1 = stop_value(&stops[i], field, gr_turn);
            return y0 + (y1 - y0) * (speed - x0) / (stops[i].speed_rps - x0);
        }
    }
    return stop_value(&stops[count - 1], field, gr_turn);
}


void charge_control_set_rules(charge_control_calibration_t *cal, charge_control_finish_t finish, real_t band_gr,
                              const charge_control_constants_t *k) {
    const charge_control_stop_t *fine = cal->fine_stops;
    uint8_t fine_count = cal->fine_stop_count;
    bool glide = finish == CHARGE_CONTROL_FINISH_GLIDE;

    cal->finish = (uint8_t) finish;
    cal->band_gr = band_gr;
    if (glide) {
        // The creep: the flow at which the powder still on its way is about the band, at the lag measured there
        real_t slowest_lag = max_f(stop_lag_s(&fine[0], cal->fine_gr_turn), REAL(CHARGE_RULE_SLOWEST_LAG_LEAST_S));
        cal->floor_rps = min_f(max_f(k->creep_band_share * band_gr / slowest_lag / cal->fine_gr_turn,
                                     REAL(CHARGE_RULE_CREEP_LEAST_SHARE) * fine[0].speed_rps),
                               fine[fine_count > CHARGE_RULE_CREEP_TOP_STOP ? CHARGE_RULE_CREEP_TOP_STOP : fine_count - 1].speed_rps);
        // The first reading at rest is meant to lie a digit under the target (half the band, if that is more)
        cal->aim_gr = -max_f(cal->digit_gr, band_gr * REAL(CHARGE_RULE_AIM_BAND_SHARE));
        cal->stop_room_gr = -cal->aim_gr;
    }
    else {
        cal->floor_rps = min_f(k->fine_top_rps, k->dribble_digits_s * cal->digit_gr / cal->fine_gr_turn);
        cal->stop_room_gr = REAL(CHARGE_RULE_JOG_STOP_SPREADS) * stop_table(fine, fine_count, STOP_SPREAD, cal->fine_gr_turn, cal->floor_rps)
                            + cal->digit_gr;
        cal->aim_gr = -cal->stop_room_gr;
    }
    // The floor is reached with twice the spread of a stop from four times its flow still to go
    cal->glide_room_gr = cal->stop_room_gr;
    if (glide) {
        cal->glide_room_gr += REAL(CHARGE_RULE_GLIDE_SPREADS) * stop_table(fine, fine_count, STOP_SPREAD, cal->fine_gr_turn,
                                                REAL(CHARGE_RULE_GLIDE_FLOW_TIMES) * cal->floor_rps);
        // On a scale so late that such a creep would take too long, the creep is quicker
        real_t stretch = cal->glide_room_gr - cal->stop_room_gr;
        cal->floor_rps = max_f(cal->floor_rps, stretch / k->creep_most_s / cal->fine_gr_turn);
    }
    real_t longest_lag = REAL(0.0);
    for (uint8_t i = (fine_count > CHARGE_RULE_CREEP_TOP_STOP ? CHARGE_RULE_CREEP_TOP_STOP : 0); i < fine_count; i++) {
        longest_lag = max_f(longest_lag, stop_lag_s(&fine[i], cal->fine_gr_turn));
    }
    cal->glide_s = min_f(REAL(CHARGE_RULE_GLIDE_MOST_S), max_f(REAL(CHARGE_RULE_GLIDE_LEAST_S), REAL(CHARGE_RULE_GLIDE_LAG_SHARE) * longest_lag));
    cal->coarse_glide_s = min_f(REAL(CHARGE_RULE_COARSE_GLIDE_MOST_S), max_f(REAL(CHARGE_RULE_COARSE_GLIDE_LEAST_S), REAL(CHARGE_RULE_GLIDE_LAG_SHARE) * stop_lag_s(&cal->coarse_stops[0],
                                                                              cal->coarse_gr_turn)));
    cal->coarse_room_gr = REAL(CHARGE_RULE_COARSE_ROOM_SPREADS) * cal->coarse_stops[0].spread_gr + cal->glide_room_gr
                          + REAL(CHARGE_RULE_COARSE_ROOM_EXTRA_GR);
    // The wait before the first judgement: the slowest the reading came to rest after a stop from near the floor
    real_t quiet = REAL(0.0);
    for (uint8_t i = 0; i < fine_count; i++) {
        if (fine[i].speed_rps <= REAL(CHARGE_RULE_REST_FLOOR_TIMES) * cal->floor_rps || i == 0) {
            quiet = max_f(quiet, fine[i].quiet_s);
        }
    }
    cal->rest_s = quiet + REAL(CHARGE_RULE_REST_EXTRA_S);
}


static int compare_float(const void *a, const void *b) {
    real_t x = *(const real_t *) a;
    real_t y = *(const real_t *) b;
    return (x > y) - (x < y);
}


void charge_control_place_stop(charge_control_calibration_t *cal, const real_t *first_errors, uint16_t count) {
    real_t sorted[32];

    if (count == 0 || count > 32) {
        return;
    }
    memcpy(sorted, first_errors, count * sizeof(real_t));
    qsort(sorted, count, sizeof(real_t), compare_float);
    real_t median = (count % 2) ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / REAL(2.0);
    if (cal->finish == CHARGE_CONTROL_FINISH_JOG && count > 1) {
        // Short by twice the rehearsals' own spread and a digit: few first readings at rest are over
        real_t mean = REAL(0.0);
        for (uint16_t i = 0; i < count; i++) {
            mean += first_errors[i];
        }
        mean /= count;
        real_t sum = REAL(0.0);
        for (uint16_t i = 0; i < count; i++) {
            sum += (first_errors[i] - mean) * (first_errors[i] - mean);
        }
        cal->aim_gr = -(REAL(CHARGE_RULE_JOG_AIM_SDS) * SQRT_R(sum / (count - 1)) + cal->digit_gr);
    }
    real_t shift = median - cal->aim_gr;
    cal->stop_room_gr += shift;         // the margins above the stop move with it
    cal->glide_room_gr += shift;
    cal->coarse_room_gr += shift;
}


void charge_control_init(charge_control_t *c, const charge_control_calibration_t *cal,
                         const charge_control_constants_t *k, real_t acceleration_rps2, bool rehearsal) {
    memset(c, 0, sizeof(*c));
    c->cal = *cal;
    c->k = *k;
    c->acceleration_rps2 = acceleration_rps2;
    c->rehearsal = rehearsal;
    c->stop_room_gr = cal->stop_room_gr;
    c->move_turns = -REAL(1.0);
    c->fine_gr_measured = cal->fine_gr_turn;
    c->fine_gr_stream = cal->fine_gr_turn;
}


void charge_control_start(charge_control_t *c, real_t target_gr, uint32_t now_ms) {
    const charge_control_calibration_t *cal = &c->cal;

    c->start_ms = now_ms;
    c->target_gr = target_gr;
    c->done = false;
    c->result = CHARGE_CONTROL_RESULT_NONE;
    c->coarse_rps = REAL(0.0);
    c->fine_rps = REAL(0.0);
    c->glide_rps[COARSE] = REAL(0.0);
    c->glide_rps[FINE] = REAL(0.0);
    c->phase = CHARGE_CONTROL_PHASE_FEED;
    // A charge too small for the coarse tube's margin is the fine tube's alone
    c->coarse_on = target_gr > cal->coarse_room_gr + REAL(CHARGE_RULE_COARSE_LEAST_TURNS) * cal->coarse_gr_turn;
    c->counted = false;
    for (int tube = COARSE; tube <= FINE; tube++) {
        c->turned[tube] = REAL(0.0);
        c->speed[tube] = REAL(0.0);
        c->marked[tube] = REAL(0.0);
        c->parcel_count[tube] = 0;
    }
    c->marked_at_s = REAL(0.0);
    c->marks = 0;
    c->last_tick_s = REAL(0.0);
    c->unseen_gr = REAL(0.0);
    c->coarse_unseen_gr = REAL(0.0);
    c->reading = REAL(0.0);
    c->have_reading = false;
    c->highest = REAL(0.0);
    c->lowest = REAL(0.0);
    c->last_rise_s = REAL(0.0);
    c->stopped_at_s = -REAL(1.0);
    c->wait_s = REAL(0.0);
    c->move_until_s = REAL(0.0);
    c->move_turns = -REAL(1.0);
    c->moves = 0;
    c->streams = 0;
    c->have_first_error = false;
    c->first_error_gr = REAL(0.0);
    c->rest_weight = REAL(0.0);
    c->coarse_ran = c->coarse_on;
    c->coarse_off_s = -REAL(1.0);
    c->window_open = false;
    c->window_reading = REAL(0.0);
    c->window_shown_turns = REAL(0.0);
    c->window_turned = REAL(0.0);
    c->window_unseen_gr = REAL(0.0);
    for (int tube = COARSE; tube <= FINE; tube++) {
        c->pid_integral[tube] = REAL(0.0);
        c->pid_last_error[tube] = REAL(0.0);
        c->pid_term[tube] = REAL(0.0);
        c->pid_held[tube] = false;
    }
    c->pid_last_s = REAL(0.0);
    c->pid_first = true;
}


static real_t since_start_s(const charge_control_t *c, uint32_t now_ms) {
    return (real_t) (uint32_t) (now_ms - c->start_ms) / REAL(1000.0);
}


static void stop_all(charge_control_t *c, charge_control_result_t result, real_t weight) {
    c->coarse_rps = REAL(0.0);
    c->fine_rps = REAL(0.0);
    c->move_turns = -REAL(1.0);
    c->coarse_on = false;
    c->done = true;
    c->result = (uint8_t) result;
    c->rest_weight = weight;
}


static void add_parcel(charge_control_t *c, int tube, real_t due_s, real_t grains) {
    uint16_t count = c->parcel_count[tube];
    if (count < CHARGE_CONTROL_PARCELS_MAX) {
        c->parcels[tube][count].due_s = due_s;
        c->parcels[tube][count].grains = grains;
        c->parcel_count[tube] = count + 1;
    }
    else {
        // No room: the newest parcel takes it, and shows no sooner than either. The forecast stays whole
        charge_control_parcel_t *last = &c->parcels[tube][count - 1];
        last->grains += grains;
        last->due_s = max_f(last->due_s, due_s);
    }
}


/*
    What each tube turned out since the last parcel: it will show after the lag of its present flow. The parcel was
    turned out over the stretch since the last one, and is counted from that stretch's middle.
*/
static void make_parcel(charge_control_t *c, real_t now_s) {
    const charge_control_calibration_t *cal = &c->cal;
    real_t turned_out_at = (c->marked_at_s + now_s) / REAL(2.0);
    real_t elapsed = now_s - c->marked_at_s;

    c->marked_at_s = now_s;
    for (int tube = COARSE; tube <= FINE; tube++) {
        real_t turned = c->turned[tube] - c->marked[tube];
        c->marked[tube] = c->turned[tube];
        if (c->counted && elapsed > REAL(0.0)) {
            c->speed[tube] = turned / elapsed;
        }
        if (turned > REAL(0.0)) {
            real_t gr_turn = tube == COARSE ? cal->coarse_gr_turn : cal->fine_gr_turn;
            real_t lag = tube == COARSE
                ? stop_table(cal->coarse_stops, cal->coarse_stop_count, STOP_LAG, gr_turn, c->speed[tube])
                : stop_table(cal->fine_stops, cal->fine_stop_count, STOP_LAG, gr_turn, c->speed[tube]);
            // The split: a fine turn before the stretch counts at the stream's figure (ADR-009, 2026-11-22)
            if (tube == FINE && c->k.learn_fine_split > REAL(0.0) && !c->window_open) {
                gr_turn = c->fine_gr_stream;
            }
            real_t grains = gr_turn * turned;
            add_parcel(c, tube, turned_out_at + lag, grains);
            c->unseen_gr += grains;
            if (tube == COARSE) {
                c->coarse_unseen_gr += grains;
            }
        }
    }
}


void charge_control_count(charge_control_t *c, uint32_t now_ms, real_t coarse_turns, real_t fine_turns) {
    (void) now_ms;
    c->turned[COARSE] = coarse_turns;
    c->turned[FINE] = fine_turns;
    c->counted = true;
}


static void end_move(charge_control_t *c, real_t now_s) {
    c->fine_rps = REAL(0.0);
    c->move_turns = -REAL(1.0);
    c->phase = CHARGE_CONTROL_PHASE_REST;
    c->stopped_at_s = now_s;
}


void charge_control_moved(charge_control_t *c, uint32_t now_ms) {
    if (c->phase == CHARGE_CONTROL_PHASE_MOVE) {
        end_move(c, since_start_s(c, now_ms));
    }
}


// The room the stream is poured against. The coarse tube's powder still unseen is known only by its turns, and a
// turn's powder varies: three times that spread of it is kept clear by both tubes
static real_t room_now(const charge_control_t *c) {
    return c->target_gr - (c->reading + c->unseen_gr) - c->k.coarse_spreads * c->cal.coarse_share * c->coarse_unseen_gr;
}


// The PID law's I and D terms on a reading, as today's loop takes them
static void pid_reading(charge_control_t *c, real_t now_s) {
    const charge_control_pid_t *pid = &c->pid;
    real_t room = room_now(c);
    const real_t errors[2] = {room - c->cal.coarse_room_gr, room};
    const real_t ki[2] = {pid->coarse_ki, pid->fine_ki};
    const real_t kd[2] = {pid->coarse_kd, pid->fine_kd};
    real_t elapsed_ms = (now_s - c->pid_last_s) * REAL(1000.0);
    for (int tube = COARSE; tube <= FINE; tube++) {
        if (!(c->k.pid_envelope && c->pid_held[tube])) {
            c->pid_integral[tube] += errors[tube];
        }
        real_t derivative = (c->pid_first || elapsed_ms <= REAL(0.0)) ? REAL(0.0) :
                            (errors[tube] - c->pid_last_error[tube]) / elapsed_ms;
        c->pid_term[tube] = ki[tube] * c->pid_integral[tube] + kd[tube] * derivative;
        c->pid_last_error[tube] = errors[tube];
    }
    c->pid_last_s = now_s;
    c->pid_first = false;
}


static void feed(charge_control_t *c, real_t now_s) {
    const charge_control_calibration_t *cal = &c->cal;
    const charge_control_constants_t *k = &c->k;
    const charge_control_pid_t *pid = k->speed_law ? &c->pid : NULL;

    real_t room = room_now(c);
    c->glide_rps[COARSE] = REAL(0.0);
    c->glide_rps[FINE] = REAL(0.0);
    if (c->coarse_on) {
        if (room <= cal->coarse_room_gr) {
            c->coarse_on = false;
            c->coarse_rps = REAL(0.0);
            if (c->coarse_off_s < REAL(0.0)) {
                c->coarse_off_s = now_s;
            }
        }
        else {
            real_t glide = (room - cal->coarse_room_gr) / (cal->coarse_gr_turn * cal->coarse_glide_s);
            c->glide_rps[COARSE] = min_f(k->coarse_top_rps, max_f(k->coarse_floor_rps, glide));
            if (pid != NULL) {
                real_t asked = pid->coarse_kp * (room - cal->coarse_room_gr) + c->pid_term[COARSE];
                real_t top = pid->coarse_max_rps;
                if (k->pid_envelope) {
                    top = min_f(min_f(top, k->coarse_top_rps), k->coarse_floor_rps + (room - cal->coarse_room_gr) /
                                                                                     (cal->coarse_gr_turn * cal->coarse_glide_s));
                }
                c->coarse_rps = min_f(top, max_f(pid->coarse_min_rps, asked));
                c->pid_held[COARSE] = !(pid->coarse_min_rps < asked && asked < top);
            }
            else {
                c->coarse_rps = c->glide_rps[COARSE];
            }
        }
    }
    if (!c->coarse_on && room <= c->stop_room_gr) {
        c->fine_rps = REAL(0.0);
        if (c->coarse_unseen_gr >= REAL(CHARGE_RULE_PAUSE_COARSE_DIGITS) * cal->digit_gr) {
            return;     // a pause, not its stop: the fine tube's stop is taken on its own powder alone
        }
        c->phase = CHARGE_CONTROL_PHASE_REST;
        c->stopped_at_s = now_s;
        c->wait_s = cal->rest_s;
        return;
    }
    real_t glide = (room - cal->glide_room_gr) / (cal->fine_gr_turn * cal->glide_s);
    c->glide_rps[FINE] = min_f(k->fine_top_rps, max_f(cal->floor_rps, glide));
    if (pid != NULL) {
        real_t asked = pid->fine_kp * room + c->pid_term[FINE];
        real_t top = pid->fine_max_rps;
        if (k->pid_envelope) {
            top = min_f(min_f(top, k->fine_top_rps), cal->floor_rps + max_f(REAL(0.0), room - c->stop_room_gr) /
                                                                       (cal->fine_gr_turn * cal->glide_s));
        }
        c->fine_rps = min_f(top, max_f(cal->floor_rps, asked));
        c->pid_held[FINE] = !(cal->floor_rps < asked && asked < top);
        return;
    }
    c->fine_rps = c->glide_rps[FINE];
}


real_t charge_control_glide_rps(const charge_control_t *c, int tube) {
    if (c->phase == CHARGE_CONTROL_PHASE_FEED) {
        return c->glide_rps[tube == COARSE ? COARSE : FINE];
    }
    return tube == COARSE ? c->coarse_rps : c->fine_rps;
}


void charge_control_tick(charge_control_t *c, uint32_t now_ms) {
    const charge_control_constants_t *k = &c->k;
    real_t now_s = since_start_s(c, now_ms);
    real_t elapsed = now_s - c->last_tick_s;

    if (c->done) {
        return;
    }
    c->last_tick_s = now_s;
    if (!c->counted) {
        // No count given: the turns reckoned from the speeds asked for, through the motors' ramp
        const real_t asked[2] = {c->coarse_rps, c->fine_rps};
        for (int tube = COARSE; tube <= FINE; tube++) {
            real_t change = c->acceleration_rps2 * elapsed;
            real_t speed = c->speed[tube];
            speed = speed < asked[tube] ? min_f(asked[tube], speed + change) : max_f(asked[tube], speed - change);
            c->speed[tube] = speed;
            c->turned[tube] += speed * elapsed;
        }
    }
    if (now_s >= (real_t) (c->marks + 1) * k->parcel_s - TIE_S) {
        make_parcel(c, now_s);
        c->marks++;
    }
    if (now_s > k->throw_most_s || c->turned[FINE] > k->fine_most_turns) {
        stop_all(c, CHARGE_CONTROL_RESULT_LIMIT, c->reading);
        return;
    }
    if (c->phase == CHARGE_CONTROL_PHASE_FEED) {
        feed(c, now_s);
    }
    else if (c->phase == CHARGE_CONTROL_PHASE_MOVE && !c->moves_by_caller && now_s >= c->move_until_s - TIE_S) {
        end_move(c, now_s);
    }
}


// The largest move that on average takes the weight no more than half way to the band's top
static const charge_control_move_t *move_for(const charge_control_t *c, real_t short_gr) {
    const charge_control_calibration_t *cal = &c->cal;
    const charge_control_move_t *chosen = &cal->moves[0];

    for (uint8_t i = 1; i < cal->move_count; i++) {
        if (cal->moves[i].mean_gr <= (short_gr + cal->band_gr) * REAL(CHARGE_RULE_TOP_UP_SHARE)) {
            chosen = &cal->moves[i];
        }
    }
    return chosen;
}


// A share of the first error at rest moves where the fine tube stops: capped, and nothing from a small one
// The fine tube's turns whose powder has not shown yet: its parcels on their way
static real_t fine_unseen_turns(const charge_control_t *c) {
    real_t grains = REAL(0.0);
    for (uint16_t i = 0; i < c->parcel_count[FINE]; i++) {
        grains += c->parcels[FINE][i].grains;
    }
    return grains / c->cal.fine_gr_turn;
}


// The fine tube's stretch begins at the first reading at which it is slowing, the coarse tube quiet if it ran
static void open_window(charge_control_t *c, real_t now_s, real_t weight) {
    const charge_control_calibration_t *cal = &c->cal;
    if (c->window_open || c->rehearsal || !(c->k.learn_fine_prior > REAL(0.0)) || c->phase != CHARGE_CONTROL_PHASE_FEED) {
        return;
    }
    if (!(c->fine_rps > REAL(0.0) && c->fine_rps < c->k.fine_top_rps)) {
        return;
    }
    if (c->coarse_ran && (c->coarse_off_s < REAL(0.0) ||
                          now_s < c->coarse_off_s + cal->coarse_stops[0].quiet_s + REAL(CHARGE_RULE_FINE_WINDOW_EXTRA_S) - TIE_S)) {
        return;
    }
    c->window_open = true;
    c->window_reading = weight;
    c->window_shown_turns = c->turned[FINE] - fine_unseen_turns(c);
    c->window_turned = c->turned[FINE];
    real_t grains = REAL(0.0);
    for (uint16_t i = 0; i < c->parcel_count[FINE]; i++) {
        grains += c->parcels[FINE][i].grains;
    }
    c->window_unseen_gr = grains;
}


static real_t learnt_clamp(const charge_control_t *c, real_t g) {
    real_t most = c->k.learn_fine_most * c->fine_gr_measured;
    return min_f(c->fine_gr_measured + most, max_f(c->fine_gr_measured - most, g));
}


// The split: the stream's figure from what showed before the stretch (the fine tube alone), the finish's from the
// stretch, the stream's powder still unseen at its start taken off
static void learn_fine_split(charge_control_t *c, real_t at_rest) {
    const charge_control_constants_t *k = &c->k;
    real_t prior = k->learn_fine_prior;
    if (!c->coarse_ran && c->window_shown_turns >= LEARN_FINE_LEAST_STREAM_TURNS && c->window_reading > REAL(0.0)) {
        c->stream_learn_n++;
        c->stream_learn_sum += c->window_reading / c->window_shown_turns;
        c->fine_gr_stream = learnt_clamp(c, (prior * c->fine_gr_measured + c->stream_learn_sum) /
                                            (prior + (real_t) c->stream_learn_n));
    }
    real_t turns = c->turned[FINE] - fine_unseen_turns(c) - c->window_turned;
    real_t gained = at_rest - c->window_reading - c->window_unseen_gr;
    if (turns < LEARN_FINE_LEAST_TURNS || gained <= REAL(0.0)) {
        return;
    }
    c->fine_learn_n++;
    c->fine_learn_sum += gained / turns;
    c->cal.fine_gr_turn = learnt_clamp(c, (prior * c->fine_gr_measured + c->fine_learn_sum) /
                                          (prior + (real_t) c->fine_learn_n));
}


// The stretch's powder over its turns, into the session's mean against the calibration's figure
static void learn_fine(charge_control_t *c, real_t at_rest) {
    const charge_control_constants_t *k = &c->k;
    if (!c->window_open || !(k->learn_fine_prior > REAL(0.0))) {
        return;
    }
    if (k->learn_fine_split > REAL(0.0)) {
        learn_fine_split(c, at_rest);
        return;
    }
    real_t turns = c->turned[FINE] - fine_unseen_turns(c) - c->window_shown_turns;
    real_t gained = at_rest - c->window_reading;
    if (turns < LEARN_FINE_LEAST_TURNS || gained <= REAL(0.0)) {
        return;
    }
    c->fine_learn_n++;
    c->fine_learn_sum += gained / turns;
    real_t mean = (k->learn_fine_prior * c->fine_gr_measured + c->fine_learn_sum) /
                  (k->learn_fine_prior + (real_t) c->fine_learn_n);
    real_t most = k->learn_fine_most * c->fine_gr_measured;
    c->cal.fine_gr_turn = min_f(c->fine_gr_measured + most, max_f(c->fine_gr_measured - most, mean));
}


static void learn(charge_control_t *c) {
    const charge_control_calibration_t *cal = &c->cal;
    const charge_control_constants_t *k = &c->k;

    if (c->rehearsal) {
        return;
    }
    real_t off = (c->first_error_gr - cal->aim_gr) / cal->digit_gr;
    if (ABS_R(off) < k->learn_dead_digits) {
        return;
    }
    off = max_f(-k->learn_most_digits, min_f(k->learn_most_digits, off));
    c->stop_room_gr += k->learn_share * off * cal->digit_gr;
}


void charge_control_reading(charge_control_t *c, uint32_t now_ms, real_t weight) {
    const charge_control_calibration_t *cal = &c->cal;
    const charge_control_constants_t *k = &c->k;
    real_t now_s = since_start_s(c, now_ms);

    if (c->done || !isfinite(weight)) {
        return;
    }
    if (weight >= c->highest + cal->digit_gr * REAL(CHARGE_RULE_RISE_DIGITS)) {
        c->highest = weight;
        c->lowest = weight;         // the lowest shown since that rise: a reading that flickers shows both
        c->last_rise_s = now_s;
    }
    c->lowest = min_f(c->lowest, weight);
    c->reading = weight;
    c->have_reading = true;
    if (c->parcel_count[COARSE] || c->parcel_count[FINE]) {
        // What was due to show by now is in this reading
        c->unseen_gr = REAL(0.0);
        c->coarse_unseen_gr = REAL(0.0);
        for (int tube = COARSE; tube <= FINE; tube++) {
            uint16_t kept = 0;
            for (uint16_t i = 0; i < c->parcel_count[tube]; i++) {
                if (c->parcels[tube][i].due_s > now_s + TIE_S) {
                    c->parcels[tube][kept++] = c->parcels[tube][i];
                    c->unseen_gr += c->parcels[tube][i].grains;
                    if (tube == COARSE) {
                        c->coarse_unseen_gr += c->parcels[tube][i].grains;
                    }
                }
            }
            c->parcel_count[tube] = kept;
        }
    }
    // Far over, in the user's terms: the tolerance is the band and half a digit (A7's conversion)
    if (weight > c->target_gr + k->over_tolerances * (cal->band_gr + cal->digit_gr / REAL(2.0))) {
        stop_all(c, CHARGE_CONTROL_RESULT_LIMIT, weight);
        return;
    }
    open_window(c, now_s, weight);
    if (k->speed_law && c->phase == CHARGE_CONTROL_PHASE_FEED) {
        pid_reading(c, now_s);
    }
    if (c->phase != CHARGE_CONTROL_PHASE_REST) {
        return;
    }
    real_t still = max_f(k->still_least_s, REAL(CHARGE_RULE_STILL_INTERVALS) * cal->interval_s);
    if (now_s < c->stopped_at_s + c->wait_s - TIE_S || now_s < c->last_rise_s + still - TIE_S) {
        return;
    }
    real_t at_rest = c->lowest;
    real_t short_gr = c->target_gr - at_rest;
    bool tries_left = c->moves + c->streams < k->most_moves;
    if (!c->rehearsal && tries_left && short_gr - c->stop_room_gr > cal->moves[cal->move_count - 1].mean_gr) {
        // Short by more than the stream's own stop margin and the largest small move together: back to the
        // stream, by the same law. Nothing is learnt from such a rest
        c->streams++;
        c->phase = CHARGE_CONTROL_PHASE_FEED;
        c->coarse_on = short_gr > cal->coarse_room_gr + REAL(CHARGE_RULE_COARSE_LEAST_TURNS) * cal->coarse_gr_turn;
        c->stopped_at_s = -REAL(1.0);
        return;
    }
    if (!c->have_first_error) {
        c->have_first_error = true;
        c->first_error_gr = at_rest - c->target_gr;
        learn(c);
        learn_fine(c, at_rest);
    }
    real_t band_edge = cal->band_gr + BAND_EDGE_DIGITS * cal->digit_gr;
    if (c->rehearsal || short_gr <= band_edge || !tries_left) {
        charge_control_result_t result = CHARGE_CONTROL_RESULT_GOOD;
        if (at_rest - c->target_gr > band_edge) {
            result = CHARGE_CONTROL_RESULT_OVER;
        }
        else if (short_gr > band_edge) {
            result = CHARGE_CONTROL_RESULT_SHORT;
        }
        stop_all(c, result, at_rest);
        return;
    }
    const charge_control_move_t *move = move_for(c, short_gr);
    c->moves++;
    c->phase = CHARGE_CONTROL_PHASE_MOVE;
    c->fine_rps = k->move_rps;
    c->move_turns = move->turns;
    c->move_until_s = now_s + move->turns / k->move_rps;
    c->wait_s = move->wait_s;
}


const char *charge_control_result_string(charge_control_result_t result) {
    switch (result) {
        case CHARGE_CONTROL_RESULT_GOOD: return "good";
        case CHARGE_CONTROL_RESULT_OVER: return "over";
        case CHARGE_CONTROL_RESULT_SHORT: return "short";
        case CHARGE_CONTROL_RESULT_LIMIT: return "limit";
        default: return "none";
    }
}
