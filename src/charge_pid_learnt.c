#include <math.h>
#include <string.h>

#include "charge_control.h"
#include "charge_pid_learnt.h"

// A rest within the band and this much of a digit is in it (the glide's band edge)
#define BAND_EDGE_DIGITS 0.25f
// A stop whose window held fewer turns teaches nothing
#define LEARN_LEAST_TURNS 0.01f


void charge_pid_default_constants(charge_pid_constants_t *k) {
    k->lag_s = 0.9f;
    k->rest_s = 2.5f;
    k->gr_turn_prior = 1.0f;
    k->learn_share = 0.15f;
    k->coarse_quiet_s = 1.0f;
    k->throw_most_s = 120.0f;
    k->over_most_gr = 3.0f;
    k->digit_gr = 0.02f;
    k->most_tops = 8;
}


static float since_start_s(const charge_pid_t *c, uint32_t now_ms) {
    return (float) (uint32_t) (now_ms - c->start_ms) / 1000.0f;
}


void charge_pid_start(charge_pid_t *c, const charge_pid_settings_t *s, const charge_pid_constants_t *k,
                      charge_pid_learning_t *learning, float target_gr, uint32_t now_ms) {
    memset(c, 0, sizeof(*c));
    c->s = *s;
    c->k = *k;
    c->learning = learning;
    if (!(learning->gr_turn > 0.0f)) {
        learning->gr_turn = k->gr_turn_prior;
        learning->learnt = 0;
    }
    c->start_ms = now_ms;
    c->target_gr = target_gr;
    c->coarse_target_gr = fmaxf(0.0f, target_gr - s->coarse_stop_threshold_gr);
    c->phase = CHARGE_PID_PHASE_STREAM;
    c->result = CHARGE_CONTROL_RESULT_NONE;
    // The glide's aim: a digit short, half the band if that is more
    c->aim_gr = -fmaxf(s->digit_gr, s->band_gr / 2.0f);
    c->stop_room_gr = -c->aim_gr;
    c->still_s = fmaxf(0.4f, 2.0f * s->interval_s);
    c->first_sample = true;
    c->coarse_moves = true;
    c->backoff_until_s = -1.0f;
    c->coarse_off_s = -1.0f;
}


void charge_pid_count(charge_pid_t *c, float fine_turns) {
    c->fine_turns = fine_turns;
}


float charge_pid_window_turns(const charge_pid_t *c) {
    uint16_t steps = (uint16_t) lroundf(c->k.lag_s / CHARGE_PID_WINDOW_STEP_S);
    if (steps < 1) {
        steps = 1;
    }
    if (steps > c->window_steps) {
        steps = c->window_steps;
    }
    float turns = c->fine_turns - c->window_base_turns;
    for (uint16_t i = 1; i <= steps; i++) {
        turns += c->window[(c->window_head + CHARGE_PID_WINDOW_STEPS - i) % CHARGE_PID_WINDOW_STEPS];
    }
    return turns;
}


static float unseen(const charge_pid_t *c) {
    return c->learning->gr_turn * charge_pid_window_turns(c);
}


static void stop_all(charge_pid_t *c, charge_control_result_t result, float weight) {
    c->coarse_rps = 0.0f;
    c->fine_rps = 0.0f;
    c->done = true;
    c->result = (uint8_t) result;
    c->rest_weight = weight;
}


void charge_pid_tick(charge_pid_t *c, uint32_t now_ms) {
    float now_s = since_start_s(c, now_ms);

    if (c->done) {
        return;
    }
    while (now_s >= c->window_at_s + CHARGE_PID_WINDOW_STEP_S) {
        c->window[c->window_head] = c->fine_turns - c->window_base_turns;
        c->window_head = (uint16_t) ((c->window_head + 1) % CHARGE_PID_WINDOW_STEPS);
        if (c->window_steps < CHARGE_PID_WINDOW_STEPS) {
            c->window_steps++;
        }
        c->window_base_turns = c->fine_turns;
        c->window_at_s += CHARGE_PID_WINDOW_STEP_S;
    }
    if (c->backoff_until_s >= 0.0f && now_s >= c->backoff_until_s) {
        c->coarse_rps = 0.0f;
        c->backoff_until_s = -1.0f;
    }
    if (c->coarse_off_s < 0.0f && !c->coarse_moves && c->backoff_until_s < 0.0f) {
        c->coarse_off_s = now_s;
    }
    if (now_s > c->k.throw_most_s) {
        stop_all(c, CHARGE_CONTROL_RESULT_LIMIT, c->reading);
    }
}


// Run the coarse trickler backwards for the stored turns, as today's coarse_trickler_backoff() does
static void backoff(charge_pid_t *c, float now_s) {
    const charge_pid_settings_t *s = &c->s;
    float speed = s->backoff_rps > 0.0f ? s->backoff_rps : s->coarse_min_rps;
    if (s->backoff_turns <= 0.0f || speed <= 0.0f) {
        return;
    }
    float time_s = fminf(15.0f, s->backoff_turns / speed);
    if (time_s < 0.001f) {
        return;
    }
    c->coarse_rps = -speed;
    c->backoff_until_s = now_s + time_s;
}


// The change of the error per millisecond, as today's loop takes it; 0 on the first reading
static float derivative(const charge_pid_t *c, float error, float last_error, float elapsed_ms) {
    if (c->first_sample || elapsed_ms <= 0.0f) {
        return 0.0f;
    }
    return (error - last_error) / elapsed_ms;
}


// One pass of today's loop on the weight given, its own fine stop taken out: the stop is the forecast's
static void legacy_pass(charge_pid_t *c, float now_s, float weight) {
    const charge_pid_settings_t *s = &c->s;
    float coarse_error = c->coarse_target_gr - weight;
    float fine_error = c->target_gr - weight;

    if (coarse_error <= 0.0f && c->coarse_moves) {
        c->coarse_moves = false;
        c->coarse_rps = 0.0f;
        backoff(c, now_s);
    }
    float elapsed_ms = (now_s - c->last_sample_s) * 1000.0f;
    c->fine_integral += fine_error;
    float speed = s->fine_kp * fine_error + s->fine_ki * c->fine_integral +
                  s->fine_kd * derivative(c, fine_error, c->fine_last_error, elapsed_ms);
    c->fine_rps = fmaxf(s->fine_min_rps, fminf(speed, s->fine_max_rps));
    if (c->coarse_moves && c->backoff_until_s < 0.0f) {
        c->coarse_integral += coarse_error;
        speed = s->coarse_kp * coarse_error + s->coarse_ki * c->coarse_integral +
                s->coarse_kd * derivative(c, coarse_error, c->coarse_last_error, elapsed_ms);
        c->coarse_rps = fmaxf(s->coarse_min_rps, fminf(speed, s->coarse_max_rps));
    }
    c->last_sample_s = now_s;
    c->fine_last_error = fine_error;
    c->coarse_last_error = coarse_error;
    c->first_sample = false;
}


static void stop_fine(charge_pid_t *c, float now_s) {
    c->fine_rps = 0.0f;
    if (c->backoff_until_s < 0.0f) {
        c->coarse_rps = 0.0f;
    }
    c->phase = CHARGE_PID_PHASE_REST;
    c->stop_s = now_s;
    c->have_highest = false;
}


static void stream(charge_pid_t *c, float now_s) {
    float forecast = c->reading + c->unseen_gr;

    if (forecast >= c->target_gr - c->stop_room_gr) {
        if (!c->have_first_error && !c->have_stop) {
            // What lands after the fine tube stops, at a pause or at its stop, is what its window held
            c->have_stop = true;
            c->stop_reading = c->reading;
            c->stop_window_turns = charge_pid_window_turns(c);
        }
        if (c->coarse_moves) {
            c->coarse_moves = false;
            c->coarse_rps = 0.0f;
            backoff(c, now_s);
            c->coarse_off_s = -1.0f;
        }
        if (c->coarse_off_s >= 0.0f && now_s >= c->coarse_off_s + c->k.coarse_quiet_s) {
            stop_fine(c, now_s);
        }
        else {
            c->fine_rps = 0.0f;     // a pause, not the stop: the coarse tube's powder may still be landing
        }
        return;
    }
    legacy_pass(c, now_s, forecast);
}


static void learn(charge_pid_t *c, float at_rest) {
    charge_pid_learning_t *l = c->learning;
    float share = fmaxf(c->k.learn_share, 1.0f / (float) (l->learnt + 1));

    if (!c->have_stop || c->stop_window_turns < LEARN_LEAST_TURNS) {
        return;
    }
    float seen = (at_rest - c->stop_reading) / c->stop_window_turns;
    seen = fminf(2.0f * l->gr_turn, fmaxf(0.5f * l->gr_turn, seen));
    l->gr_turn += share * (seen - l->gr_turn);
    l->learnt++;
}


static void rest(charge_pid_t *c, float now_s) {
    if (now_s < c->stop_s + c->k.rest_s || now_s < c->last_rise_s + c->still_s) {
        return;
    }
    float at_rest = c->lowest;
    float error = at_rest - c->target_gr;
    if (!c->have_first_error) {
        c->have_first_error = true;
        c->first_error_gr = error;
        learn(c, at_rest);
    }
    float band_edge = c->s.band_gr + BAND_EDGE_DIGITS * c->s.digit_gr;
    if (-error > band_edge && c->tops < c->k.most_tops) {
        // The top-up is the stream again: today's loop gives the fine tube its least speed or more
        c->tops++;
        c->phase = CHARGE_PID_PHASE_STREAM;
        return;
    }
    stop_all(c, error > band_edge ? CHARGE_CONTROL_RESULT_OVER :
                -error > band_edge ? CHARGE_CONTROL_RESULT_SHORT : CHARGE_CONTROL_RESULT_GOOD, at_rest);
}


void charge_pid_reading(charge_pid_t *c, uint32_t now_ms, float weight) {
    float now_s = since_start_s(c, now_ms);

    if (c->done || !isfinite(weight)) {
        return;
    }
    c->reading = weight;
    c->unseen_gr = unseen(c);
    if (!c->have_highest || weight >= c->highest + c->s.digit_gr / 2.0f) {
        c->have_highest = true;
        c->highest = weight;
        c->lowest = weight;
        c->last_rise_s = now_s;
    }
    c->lowest = fminf(c->lowest, weight);
    if (weight > c->target_gr + c->k.over_most_gr) {
        stop_all(c, CHARGE_CONTROL_RESULT_LIMIT, weight);
        return;
    }
    if (c->phase == CHARGE_PID_PHASE_STREAM) {
        stream(c, now_s);
    }
    else {
        rest(c, now_s);
    }
}
