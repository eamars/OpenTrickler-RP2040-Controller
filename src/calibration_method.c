/*
 * ADR-009 step 1: the calibration's method (calibration_method.h), as tools/charge_sim/calibration.py runs it. Each
 * number and each step is that file's: a change to one is made to both, and tests/calibration_sim.py runs the two on
 * the simulation side by side.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "calibration_method.h"

// The method's own numbers (calibration.py's)
#define CALIBRATION_DEFAULT_CAPACITY 100.0f     // the cup's, when the cup's part never ran
#define DEFAULT_WATCH_S 4.0f            // a stop watched this long on a scale never calibrated: on the slow side
#define PRIME_RUNS_MOST 5               // a tube turned at most this many times until two runs pour alike
#define PRIME_ALIKE_SHARE 0.1f          // two runs pour alike within this share of the larger
#define PRIME_WATCH_S 2.0f
#define POUR_PER_TURN_MOST 12.0f        // a run's most pour for the cup's check, before the tube has poured at all
#define SCALE_PRIME_RPS 0.5f            // the fine tube's prime in the scale part
#define SCALE_COARSE_PRIME_RPS 1.0f     // the coarse tube's, before its lag's runs
#define SETTLE_BURST_TURNS 0.2f         // the settle time's burst of the fine tube: 0.1 s at 2 rev/s, its powder
#define SETTLE_BURSTS 4                 // turned out at once, several steps of it (about 0.16 gr), so what follows
#define SETTLE_BURST_RPS 2.0f           // its first move is the scale's (the owner, 2026-11-25)
#define SETTLE_WATCH_S 10.0f            // each move watched this long: longer than any scale takes to come to rest
#define WATCH_MARGIN_S 1.0f             // a stop is watched for the slowest lag and the settle time, and this
#define LAG_RUNS_EACH 2
#define LAST_S 0.6f                     // the reading at rest is the median of the watch's last stretch
#define SPEED_LADDER 6                  // speeds from the motor's least up to the prime's speed, below it
#define SPEED_STOPS 4                   // stops at each speed
#define SPEED_RUN_TURNS 0.1f            // every stop's run turns this much,
#define SPEED_RUN_LEAST_S 0.5f          // and lasts at least this long,
#define SPEED_RUN_MOST_S 10.0f          // and never longer than this
#define LEAST_TIMES 2.0f                // the least speed is the measured minimum times this
#define REGISTER_SHARE 0.5f             // a speed pours when its runs pour at least this share of the prime's rate

static const float prime_turns[2] = {1.0f, 3.0f};      // coarse, fine

// The lag's runs, (speed rev/s, run s), fine then coarse: about 0.2, 0.4 and 0.8 weight a second on the fine tube,
// 3 and 10 on the coarse (progress, 2026-11-25)
typedef struct {
    int tube;
    float speed_rps;
    float run_s;
} lag_run_t;
static const lag_run_t lag_runs[CALIBRATION_LAG_POINTS] = {
    {CALIBRATION_FINE, 0.25f, 2.0f}, {CALIBRATION_FINE, 0.5f, 2.0f}, {CALIBRATION_FINE, 1.0f, 1.5f},
    {CALIBRATION_COARSE, 0.25f, 1.5f}, {CALIBRATION_COARSE, 1.0f, 1.0f},
};

// One run and its stop, from rest to rest
typedef struct {
    float before;               // the run's first reading
    float rest;                 // the reading at rest after its stop
    float gain;
    float first_s;              // the first move after the stop; NAN when none
    float settle_s;             // when it came to rest after the stop; 0 when it never moved
    float turns;
    float run_s;
} run_t;

// The readings of the run now and of its watch, in the bench's room: one run at a time
static calibration_sample_t *during;
static calibration_sample_t *after;
static int during_count;
static int after_count;
static float last_reading = 0.0f;

// The cup: its capacity, and the powder in it now, counted by what each run poured
static float cup_capacity = CALIBRATION_DEFAULT_CAPACITY;
static float cup_held = 0.0f;

// What the part has spent
static float part_seconds;
static float part_poured;


void calibration_cup(float capacity, float held_now) {
    cup_capacity = isfinite(capacity) && capacity > 0.0f ? capacity : CALIBRATION_DEFAULT_CAPACITY;
    cup_held = isfinite(held_now) ? held_now : 0.0f;
}


static float median(float *values, int count) {
    // A few values: sorted in place by insertion
    for (int i = 1; i < count; i += 1) {
        float value = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > value) {
            values[j + 1] = values[j];
            j -= 1;
        }
        values[j + 1] = value;
    }
    return count % 2 == 1 ? values[count / 2] : (values[count / 2 - 1] + values[count / 2]) / 2.0f;
}


void calibration_settled(const calibration_sample_t *samples, int count, float watch_s, float *rest, float *first,
                         float *settle) {
    *first = NAN;
    *settle = 0.0f;
    if (count <= 0) {
        *rest = last_reading;
        return;
    }
    float last[32];             // the last stretch's readings: 0.6 s, a few at any scale's rate; its latest 32
    int last_count = 0;
    for (int i = 0; i < count; i += 1) {
        if (samples[i].t_s >= watch_s - LAST_S) {
            if (last_count == 32) {
                memmove(last, last + 1, sizeof(float) * 31);
                last_count = 31;
            }
            last[last_count++] = samples[i].weight;
        }
    }
    *rest = last_count > 0 ? median(last, last_count) : samples[count - 1].weight;
    // A step is the smallest change the readings make
    float step = INFINITY;
    for (int i = 1; i < count; i += 1) {
        float change = fabsf(samples[i].weight - samples[i - 1].weight);
        if (change > 0.0f && change < step) {
            step = change;
        }
        if (isnan(*first) && samples[i].weight != samples[0].weight) {
            *first = samples[i].t_s;
        }
    }
    if (!isfinite(step)) {
        return;                 // it never moved
    }
    // When it first shows its rest and never again strays more than one step from it: a flicker comes back, a
    // change's last step stays
    int last_away = -1;
    for (int i = 0; i < count; i += 1) {
        if (fabsf(samples[i].weight - *rest) > 1.5f * step) {
            last_away = i;
        }
    }
    for (int i = last_away + 1; i < count; i += 1) {
        if (fabsf(samples[i].weight - *rest) <= 0.5f * step + 1e-6f) {
            *settle = samples[i].t_s;
            return;
        }
    }
    *settle = samples[last_away + 1 < count ? last_away + 1 : count - 1].t_s;
}


/*
 * One steady turn and its stop, watched to rest. False when stopped. Before it, a run whose most pour would pass the
 * cup's capacity pauses the calibration for the cup to be emptied.
 */
static bool run_and_stop(const calibration_bench_t *bench, int tube, float speed, float run_s, float watch_s,
                         float per_turn, run_t *run) {
    float most = speed * run_s * per_turn;
    if (cup_held + most > cup_capacity) {
        if (!bench->cup_full(bench->ctx, cup_held, most)) {
            return false;
        }
        cup_held = 0.0f;
    }
    during = bench->during;
    after = bench->after;
    bench->set_speed(bench->ctx, tube, speed);
    during_count = bench->watch(bench->ctx, run_s, during, CALIBRATION_DURING_MAX);
    bench->set_speed(bench->ctx, tube, 0.0f);
    if (during_count < 0) {
        return false;
    }
    after_count = bench->watch(bench->ctx, watch_s, after, CALIBRATION_AFTER_MAX);
    if (after_count < 0) {
        return false;
    }
    run->before = during_count > 0 ? during[0].weight : last_reading;
    calibration_settled(after, after_count, watch_s, &run->rest, &run->first_s, &run->settle_s);
    if (after_count > 0) {
        last_reading = after[after_count - 1].weight;
    }
    run->gain = run->rest - run->before;
    run->turns = speed * run_s;
    run->run_s = run_s;
    cup_held += run->gain;
    part_seconds += run_s + watch_s;
    part_poured += run->gain;
    return true;
}


// Turn a tube until two runs pour alike: its weight a turn from the last run. NAN when stopped
static float prime(const calibration_bench_t *bench, int tube, float speed) {
    run_t run = {.gain = 0.0f, .turns = prime_turns[tube]};
    bool has_last = false;
    float last = 0.0f;
    for (int idx = 0; idx < PRIME_RUNS_MOST; idx += 1) {
        if (!run_and_stop(bench, tube, speed, prime_turns[tube] / speed, PRIME_WATCH_S, POUR_PER_TURN_MOST, &run)) {
            return NAN;
        }
        if (has_last && fabsf(run.gain - last) <= PRIME_ALIKE_SHARE * fmaxf(run.gain, last)) {
            break;
        }
        last = run.gain;
        has_last = true;
    }
    return run.turns > 0.0f ? fmaxf(run.gain, 0.0f) / run.turns : 0.0f;
}


/*
 * How far the reading ran behind the powder turned out, summed over a run and its stop, in weight-seconds
 * (staged.py's `behind`): the powder taken as turned out evenly from the run's start to its stop, the readings capped
 * at the rest, up to `quiet` after the stop
 */
static float behind(const run_t *run, float quiet) {
    float poured = run->rest - run->before;
    float area = 0.0f;
    bool has_prev = false;
    float t0 = 0.0f;
    float gap0 = 0.0f;
    for (int i = 0; i < during_count + after_count; i += 1) {
        bool in_run = i < during_count;
        const calibration_sample_t *sample = in_run ? &during[i] : &after[i - during_count];
        if (!in_run && sample->t_s > quiet) {
            continue;
        }
        float t = in_run ? sample->t_s : run->run_s + sample->t_s;
        float shown = fminf(sample->weight, run->rest) - run->before;
        float out = poured * fminf(1.0f, fmaxf(0.0f, t / run->run_s));
        float gap = out - shown;
        if (has_prev) {
            area += (gap0 + gap) / 2.0f * (t - t0);
        }
        t0 = t;
        gap0 = gap;
        has_prev = true;
    }
    return area;
}


static void say_failure(char *failure, const char *text) {
    if (failure[0] == '\0') {
        snprintf(failure, CALIBRATION_FAILURE_LEN, "%s", text);
    }
}


calibration_outcome_t calibration_scale_part(const calibration_bench_t *bench, calibration_scale_record_t *record) {
    memset(record, 0, sizeof(*record));
    record->settle_s = NAN;
    part_seconds = 0.0f;
    part_poured = 0.0f;
    bench->report(bench->ctx, 0, "Priming the fine tube");
    float fine_turn = prime(bench, CALIBRATION_FINE, SCALE_PRIME_RPS);
    if (isnan(fine_turn)) {
        return CALIBRATION_STOPPED;
    }
    if (fine_turn <= 0.0f) {
        say_failure(record->failure, "Fine tube: no powder");
        record->seconds = part_seconds;
        record->poured = part_poured;
        return CALIBRATION_DONE;
    }

    // The settle time: bursts, each watched long; the median from a burst's first move to its rest, of those that
    // moved more than one step
    float settles[SETTLE_BURSTS];
    int moved = 0;
    float longest_rest = 0.0f;
    for (int idx = 0; idx < SETTLE_BURSTS; idx += 1) {
        bench->report(bench->ctx, 5 + idx * 5, "Bursts");
        run_t run;
        if (!run_and_stop(bench, CALIBRATION_FINE, SETTLE_BURST_RPS, SETTLE_BURST_TURNS / SETTLE_BURST_RPS,
                          SETTLE_WATCH_S, fine_turn, &run)) {
            return CALIBRATION_STOPPED;
        }
        if (!isnan(run.first_s) && run.settle_s > 0.0f) {
            settles[moved++] = fmaxf(0.0f, run.settle_s - run.first_s);
            longest_rest = fmaxf(longest_rest, run.settle_s);
        }
    }
    float watch_s = DEFAULT_WATCH_S;
    if (moved > 0) {
        record->settle_s = median(settles, moved);
        watch_s = longest_rest + WATCH_MARGIN_S;
    }
    else {
        say_failure(record->failure, "Bursts: no change");
    }

    // The lag at each flow, both tubes: the coarse primed first
    float coarse_turn = NAN;
    for (int idx = 0; idx < CALIBRATION_LAG_POINTS; idx += 1) {
        const lag_run_t *planned = &lag_runs[idx];
        bench->report(bench->ctx, 25 + idx * 15, "The scale's lag");
        float per_turn = fine_turn;
        if (planned->tube == CALIBRATION_COARSE) {
            if (isnan(coarse_turn)) {
                coarse_turn = prime(bench, CALIBRATION_COARSE, SCALE_COARSE_PRIME_RPS);
                if (isnan(coarse_turn)) {
                    return CALIBRATION_STOPPED;
                }
            }
            per_turn = coarse_turn;
        }
        float flow_sum = 0.0f;
        float lag_sum = 0.0f;
        int made = 0;
        for (int each = 0; each < LAG_RUNS_EACH; each += 1) {
            run_t run;
            if (!run_and_stop(bench, planned->tube, planned->speed_rps, planned->run_s, watch_s, per_turn, &run)) {
                return CALIBRATION_STOPPED;
            }
            if (run.gain > 0.0f) {
                flow_sum += run.gain / planned->run_s;
                lag_sum += behind(&run, run.settle_s > 0.0f ? run.settle_s : watch_s) / run.gain;
                made += 1;
            }
        }
        if (made == 0) {
            say_failure(record->failure, planned->tube == CALIBRATION_FINE ? "Fine tube: no powder"
                                                                             : "Coarse tube: no powder");
            continue;
        }
        // Slowest first, as measured
        float flow = flow_sum / (float) made;
        float lag = fmaxf(0.0f, lag_sum / (float) made);
        int at = record->lag_count;
        while (at > 0 && record->lag_flow_per_s[at - 1] > flow) {
            record->lag_flow_per_s[at] = record->lag_flow_per_s[at - 1];
            record->lag_s[at] = record->lag_s[at - 1];
            at -= 1;
        }
        record->lag_flow_per_s[at] = flow;
        record->lag_s[at] = lag;
        record->lag_count += 1;
    }
    bench->report(bench->ctx, 100, "Done");
    record->seconds = part_seconds;
    record->poured = part_poured;
    return CALIBRATION_DONE;
}


float calibration_watch_after_stop(const calibration_scale_record_t *scale) {
    if (scale == NULL || scale->lag_count == 0 || isnan(scale->settle_s)) {
        return DEFAULT_WATCH_S;
    }
    float slowest = 0.0f;
    for (int idx = 0; idx < scale->lag_count; idx += 1) {
        slowest = fmaxf(slowest, scale->lag_s[idx]);
    }
    return slowest + scale->settle_s + WATCH_MARGIN_S;
}


// A speed's weight a turn by its runs' sum, the run farthest from their median (a clump or a lull) left out
static float pour_rate(const float *gains, const float *turns, int count) {
    float sorted[SPEED_STOPS];
    memcpy(sorted, gains, sizeof(float) * (size_t) count);
    float middle = median(sorted, count);
    int out = 0;
    for (int idx = 1; idx < count; idx += 1) {
        if (fabsf(gains[idx] - middle) > fabsf(gains[out] - middle)) {
            out = idx;
        }
    }
    float kept_gain = 0.0f;
    float kept_turns = 0.0f;
    for (int idx = 0; idx < count; idx += 1) {
        if (idx != out) {
            kept_gain += gains[idx];
            kept_turns += turns[idx];
        }
    }
    return kept_turns > 0.0f ? kept_gain / kept_turns : 0.0f;
}


calibration_outcome_t calibration_speeds_part(const calibration_bench_t *bench, int tube, float least_rps,
                                              float most_rps, const calibration_scale_record_t *scale,
                                              calibration_speeds_record_t *record) {
    memset(record, 0, sizeof(*record));
    record->least_rps = NAN;
    record->stream_per_turn = NAN;
    part_seconds = 0.0f;
    part_poured = 0.0f;
    char line[40];
    const char *named = tube == CALIBRATION_FINE ? "Fine tube" : "Coarse tube";
    float watch_s = calibration_watch_after_stop(scale);
    float prime_rps = sqrtf(least_rps * most_rps);
    snprintf(line, sizeof(line), "%s %.2f rev/s", named, (double) prime_rps);
    bench->report(bench->ctx, 0, line);
    float stream = prime(bench, tube, prime_rps);
    if (isnan(stream)) {
        return CALIBRATION_STOPPED;
    }
    record->stream_per_turn = stream;
    if (stream <= 0.0f) {
        snprintf(record->failure, CALIBRATION_FAILURE_LEN, "%s: no powder", named);
        record->seconds = part_seconds;
        record->poured = part_poured;
        return CALIBRATION_DONE;
    }

    // The ladder, from the motor's least up to the prime's speed, below it: the slowest speed that pours
    float ratio = powf(prime_rps / least_rps, 1.0f / (float) SPEED_LADDER);
    float slowest = NAN;
    float fastest = NAN;
    for (int step = 0; step < SPEED_LADDER; step += 1) {
        float speed = least_rps * powf(ratio, (float) step);
        float run_s = fminf(SPEED_RUN_MOST_S, fmaxf(SPEED_RUN_LEAST_S, SPEED_RUN_TURNS / speed));
        float gains[SPEED_STOPS];
        float turns[SPEED_STOPS];
        bool any = false;
        snprintf(line, sizeof(line), "%s %.2f rev/s", named, (double) speed);
        for (int stop = 0; stop < SPEED_STOPS; stop += 1) {
            bench->report(bench->ctx, 10 + (step * SPEED_STOPS + stop) * 90 / (SPEED_LADDER * SPEED_STOPS), line);
            run_t run;
            if (!run_and_stop(bench, tube, speed, run_s, watch_s, stream, &run)) {
                return CALIBRATION_STOPPED;
            }
            gains[stop] = run.gain;
            turns[stop] = run.turns;
            any = any || run.gain > 0.0f;
        }
        if (!any) {
            continue;           // it poured nothing at this speed
        }
        fastest = speed;
        if (isnan(slowest) && pour_rate(gains, turns, SPEED_STOPS) >= REGISTER_SHARE * stream) {
            slowest = speed;
        }
    }
    bench->report(bench->ctx, 100, "Done");
    record->seconds = part_seconds;
    record->poured = part_poured;
    if (isnan(slowest)) {
        snprintf(record->failure, CALIBRATION_FAILURE_LEN, "%s: too little powder when slow", named);
        return CALIBRATION_DONE;
    }
    record->least_rps = fminf(LEAST_TIMES * slowest, fastest);
    return CALIBRATION_DONE;
}
