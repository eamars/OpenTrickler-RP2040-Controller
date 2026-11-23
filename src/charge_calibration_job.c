#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "charge_calibration_job.h"
#include "charge_rules.h"

/*
    A port of tools/charge_sim/staged.py's measure_on (ADR-009, gate 3), in its order, so the two read side by side.
    Where this differs, it says so: the runs are asked one at a time, and their readings given back, where the
    reference calls its bench.
*/

typedef charge_control_real_t real_t;
#define REAL(x) ((real_t) (x))
#ifdef CHARGE_CONTROL_DOUBLE
#define SQRT_R sqrt
#else
#define SQRT_R sqrtf
#endif

#define COARSE 0
#define FINE 1

typedef enum {
    PHASE_PRIME = 0,
    PHASE_PROBES,
    PHASE_COARSE_FIGURE,
    PHASE_FINE_FIGURE,
    PHASE_FINE_STOPS,
    PHASE_FINE_TOP,
    PHASE_COARSE_STOPS,
    PHASE_MOVES,
    PHASE_BACKOFF,
    PHASE_DONE,
} phase_t;

// The fine stops' flows in digits a second, their counts and run lengths (F1); the coarse stops' counts and run lengths
// at the user's least, the geometric mean of its ends and its most (F2); the small moves' sizes in digits and counts (F3)
static const real_t fine_rates_digits_s[] = CHARGE_CAL_FINE_RATES_DIGITS_S;
static const uint8_t fine_counts[] = CHARGE_CAL_FINE_COUNTS;
static const real_t fine_run_s[] = CHARGE_CAL_FINE_RUN_S;
#define FINE_RATES (sizeof(fine_counts) / sizeof(fine_counts[0]))
static const uint8_t coarse_counts[] = CHARGE_CAL_COARSE_COUNTS;
static const real_t coarse_run_s[] = CHARGE_CAL_COARSE_RUN_S;
#define COARSE_STOPS (sizeof(coarse_counts) / sizeof(coarse_counts[0]))
static const real_t move_digits[] = CHARGE_CAL_MOVE_DIGITS;
static const uint8_t move_counts[] = CHARGE_CAL_MOVE_COUNTS;
#define MOVE_SIZES (sizeof(move_counts) / sizeof(move_counts[0]))


static real_t max_f(real_t a, real_t b) {
    return a > b ? a : b;
}


static real_t min_f(real_t a, real_t b) {
    return a < b ? a : b;
}


static int compare_real(const void *a, const void *b) {
    real_t x = *(const real_t *) a;
    real_t y = *(const real_t *) b;
    return (x > y) - (x < y);
}


// The middle value, the two middle ones' mean for an even count (Python's statistics.median); sorts the values given
static real_t median_of(real_t *values, uint16_t count) {
    qsort(values, count, sizeof(real_t), compare_real);
    return (count % 2) ? values[count / 2] : (values[count / 2 - 1] + values[count / 2]) / REAL(2.0);
}


// staged.py's percentile: the value at that share of the sorted values
static real_t percentile_of(real_t *values, uint16_t count, real_t share) {
    qsort(values, count, sizeof(real_t), compare_real);
    uint16_t at = (uint16_t) (share * (real_t) count);
    return values[at < count ? at : count - 1];
}


static real_t mean_of(const real_t *values, uint16_t count) {
    real_t sum = REAL(0.0);
    for (uint16_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / (real_t) count;
}


// The sample standard deviation (Python's statistics.stdev)
static real_t sd_of(const real_t *values, uint16_t count) {
    real_t mean = mean_of(values, count);
    real_t sum = REAL(0.0);
    for (uint16_t i = 0; i < count; i++) {
        sum += (values[i] - mean) * (values[i] - mean);
    }
    return SQRT_R(sum / (real_t) (count - 1));
}


void charge_cal_job_begin(charge_cal_job_t *job, real_t digit_gr, real_t interval_s, const charge_control_constants_t *k) {
    memset(job, 0, sizeof(*job));
    job->digit_gr = digit_gr;
    job->interval_s = interval_s;
    job->k = *k;
    job->phase = PHASE_PRIME;
    job->tube = COARSE;
    job->last_gain = -REAL(1.0);
}


static void ask(charge_cal_job_t *job, uint8_t tube, real_t speed, real_t run_s, real_t watch_s) {
    job->asked = (charge_cal_run_t) {tube, speed, run_s, watch_s, false, REAL(0.0), REAL(0.0)};
    job->reading_count = 0;
}


// The entry's next run, or false when its runs are made
static bool entry_next(charge_cal_job_t *job, uint8_t tube, real_t watch_s) {
    if (job->run >= job->count) {
        return false;
    }
    ask(job, tube, job->speed, job->run_s, watch_s);
    return true;
}


static void begin_entry(charge_cal_job_t *job, real_t speed, uint8_t count, real_t run_s) {
    job->speed = speed;
    job->count = count;
    job->run_s = run_s;
    job->run = 0;
}


// Each tube's stops from one speed, from the entry's runs (staged.py's stops_at)
static charge_control_stop_t stop_of(const charge_cal_job_t *job) {
    real_t rises[CHARGE_CAL_JOB_RUNS_MAX];
    real_t quiets[CHARGE_CAL_JOB_RUNS_MAX];
    real_t poured = REAL(0.0);
    real_t behind = REAL(0.0);
    for (uint8_t i = 0; i < job->count; i++) {
        rises[i] = job->runs[i].rise;
        quiets[i] = job->runs[i].quiet;
        poured += job->runs[i].gain;
        behind += job->runs[i].behind;
    }
    real_t lag = poured >= job->digit_gr ? max_f(REAL(0.0), behind / poured) : -REAL(1.0);
    return (charge_control_stop_t) {job->speed, mean_of(rises, job->count), sd_of(rises, job->count),
                                    percentile_of(quiets, job->count, REAL(CHARGE_CAL_SPREAD_SHARE)), lag};
}


// The fine ladder's next stop: its speed in digits a second over the figure, inside the user's range; one only when
// faster than the last (staged.py: `if not fine_stops or speed > fine_stops[-1].speed`)
static bool next_fine_stop(charge_cal_job_t *job) {
    charge_control_calibration_t *cal = &job->cal;
    while (job->index < FINE_RATES) {
        real_t speed = min_f(job->k.fine_most_rps, max_f(job->k.fine_least_rps,
                                                         fine_rates_digits_s[job->index] * job->digit_gr / cal->fine_gr_turn));
        uint8_t entry = job->index++;
        if (cal->fine_stop_count == 0 || speed > cal->fine_stops[cal->fine_stop_count - 1].speed_rps) {
            begin_entry(job, speed, fine_counts[entry], fine_run_s[entry]);
            return true;
        }
    }
    return false;
}


// A tube's prime speed: the geometric mean of its user range (F6, gate 3)
static real_t prime_rps(const charge_cal_job_t *job, uint8_t tube) {
    return tube == COARSE ? SQRT_R(job->k.coarse_least_rps * job->k.coarse_most_rps)
                          : SQRT_R(job->k.fine_least_rps * job->k.fine_most_rps);
}


static real_t coarse_speed(const charge_cal_job_t *job, uint8_t index) {
    real_t least = job->k.coarse_least_rps;
    real_t most = job->k.coarse_most_rps;
    return index == 0 ? least : index == 1 ? SQRT_R(least * most) : most;
}


bool charge_cal_job_next(charge_cal_job_t *job, charge_cal_run_t *run) {
    charge_control_calibration_t *cal = &job->cal;
    while (true) {
        switch ((phase_t) job->phase) {
            case PHASE_PRIME: {
                // Until two runs pour the same, not by a count: a coarse turn, then three fine turns, at the prime's speed
                if (job->run < CHARGE_CAL_PRIME_TRIES) {
                    // At the geometric mean of the tube's user range (F6, gate 3)
                    real_t turns = job->tube == COARSE ? REAL(CHARGE_CAL_PRIME_COARSE_TURNS) : REAL(CHARGE_CAL_PRIME_FINE_TURNS);
                    real_t speed = prime_rps(job, job->tube);
                    ask(job, job->tube, speed, turns / speed, REAL(CHARGE_CAL_PRIME_WATCH_S));
                    *run = job->asked;
                    return true;
                }
                // then the next tube
                if (job->tube == COARSE) {
                    job->tube = FINE;
                    job->run = 0;
                    job->last_gain = -REAL(1.0);
                    continue;
                }
                job->phase = PHASE_PROBES;
                // At the fine prime's speed: no ladder is measured yet
                begin_entry(job, prime_rps(job, FINE), CHARGE_CAL_PROBE_MOVES,
                            REAL(CHARGE_CAL_PROBE_TURNS) / prime_rps(job, FINE));
                continue;
            }
            case PHASE_PROBES:
                if (entry_next(job, FINE, REAL(CHARGE_CAL_PROBE_WATCH_S))) {
                    *run = job->asked;
                    return true;
                }
                // How long this scale takes to show a small move: every watch below follows from it
                cal->watch_s = max_f(REAL(CHARGE_CAL_LEAST_WATCH_S),
                                     REAL(CHARGE_CAL_WATCH_TIMES) * job->probe_quiet_most + REAL(CHARGE_CAL_WATCH_EXTRA_S));
                job->phase = PHASE_COARSE_FIGURE;
                begin_entry(job, REAL(CHARGE_CAL_FIGURE_RPS), CHARGE_CAL_FIGURE_RUNS, REAL(CHARGE_CAL_COARSE_FIGURE_RUN_S));
                continue;
            case PHASE_COARSE_FIGURE:
                if (entry_next(job, COARSE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                job->phase = PHASE_FINE_FIGURE;
                begin_entry(job, REAL(CHARGE_CAL_FIGURE_RPS), CHARGE_CAL_FIGURE_RUNS, REAL(CHARGE_CAL_FINE_FIGURE_RUN_S));
                continue;
            case PHASE_FINE_FIGURE:
                if (entry_next(job, FINE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                cal->coarse_gr_turn = job->coarse_gain_sum / job->coarse_turn_sum;
                cal->fine_gr_turn = job->fine_gain_sum / job->fine_turn_sum;
                job->phase = PHASE_FINE_STOPS;
                job->index = 0;
                if (!next_fine_stop(job)) {
                    job->phase = PHASE_FINE_TOP;
                }
                continue;
            case PHASE_FINE_STOPS:
                if (entry_next(job, FINE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                if (cal->fine_stop_count < CHARGE_CONTROL_FINE_STOPS_MAX) {
                    cal->fine_stops[cal->fine_stop_count++] = stop_of(job);
                }
                if (!next_fine_stop(job)) {
                    // One at the fine most if the ladder ends below it
                    job->phase = PHASE_FINE_TOP;
                    begin_entry(job, job->k.fine_most_rps, CHARGE_CAL_FINE_TOP_COUNT, REAL(CHARGE_CAL_FINE_TOP_RUN_S));
                    job->count = cal->fine_stops[cal->fine_stop_count - 1].speed_rps < job->k.fine_most_rps ?
                                 CHARGE_CAL_FINE_TOP_COUNT : 0;
                }
                continue;
            case PHASE_FINE_TOP:
                if (entry_next(job, FINE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                if (job->count > 0 && cal->fine_stop_count < CHARGE_CONTROL_FINE_STOPS_MAX) {
                    cal->fine_stops[cal->fine_stop_count++] = stop_of(job);
                }
                job->phase = PHASE_COARSE_STOPS;
                job->index = 0;
                begin_entry(job, coarse_speed(job, 0), coarse_counts[0], coarse_run_s[0]);
                continue;
            case PHASE_COARSE_STOPS:
                if (entry_next(job, COARSE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                cal->coarse_stops[cal->coarse_stop_count++] = stop_of(job);
                if (++job->index < COARSE_STOPS) {
                    begin_entry(job, coarse_speed(job, job->index), coarse_counts[job->index], coarse_run_s[job->index]);
                    continue;
                }
                job->phase = PHASE_MOVES;
                job->index = 0;
                // The small moves at the ladder's slowest stop, beside the creep a throw makes them at (C1, gate 3)
                begin_entry(job, cal->fine_stops[0].speed_rps, move_counts[0],
                            move_digits[0] * job->digit_gr / cal->fine_gr_turn / cal->fine_stops[0].speed_rps);
                continue;
            case PHASE_MOVES: {
                if (entry_next(job, FINE, cal->watch_s)) {
                    *run = job->asked;
                    return true;
                }
                // The move is the turn the tube really made, by the unit's own count: a throw's move is made to that
                real_t gains[CHARGE_CAL_JOB_RUNS_MAX];
                real_t turned[CHARGE_CAL_JOB_RUNS_MAX];
                real_t shown[CHARGE_CAL_JOB_RUNS_MAX];
                uint8_t shown_count = 0;
                for (uint8_t i = 0; i < job->count; i++) {
                    gains[i] = job->runs[i].gain;
                    turned[i] = job->runs[i].turns;
                    if (job->runs[i].gain >= job->digit_gr / REAL(2.0)) {
                        shown[shown_count++] = job->runs[i].quiet;
                    }
                }
                real_t wait = shown_count > 0 ? percentile_of(shown, shown_count, REAL(CHARGE_CAL_SPREAD_SHARE)) +
                                                REAL(CHARGE_CAL_MOVE_WAIT_EXTRA_S) : cal->watch_s;
                real_t mean_gain = mean_of(gains, job->count);
                cal->moves[cal->move_count++] = (charge_control_move_t) {
                    mean_of(turned, job->count), mean_gain, percentile_of(gains, job->count, REAL(CHARGE_CAL_SPREAD_SHARE)), wait};
                if (++job->index < MOVE_SIZES) {
                    begin_entry(job, cal->fine_stops[0].speed_rps, move_counts[job->index],
                                move_digits[job->index] * job->digit_gr / cal->fine_gr_turn / cal->fine_stops[0].speed_rps);
                    continue;
                }
                // The coarse tube's powder a turn, run by run: its spread is how far a count of its turns can be off
                cal->coarse_share = sd_of(job->coarse_yields, job->coarse_yield_count) /
                                    mean_of(job->coarse_yields, job->coarse_yield_count);
                cal->digit_gr = job->digit_gr;
                cal->interval_s = job->interval_s;
                cal->backoff_unseen_gr = -REAL(1.0);
                cal->backoff_spread_gr = -REAL(1.0);
                cal->backoff_turns = REAL(0.0);
                cal->backoff_rps = REAL(0.0);
                job->phase = PHASE_BACKOFF;
                // The user's back-off on: its spill measured as it is set, from the coarse floor (A11)
                begin_entry(job, job->k.coarse_least_rps,
                            job->k.user_backoff_turns > REAL(0.0) && job->k.user_backoff_rps > REAL(0.0) ?
                            CHARGE_CAL_BACKOFF_STOPS : 0, coarse_run_s[0]);
                continue;
            }
            case PHASE_BACKOFF:
                if (job->run < job->count) {
                    ask(job, COARSE, job->speed, job->run_s, cal->watch_s);
                    job->asked.backoff = true;
                    job->asked.backoff_turns = job->k.user_backoff_turns;
                    job->asked.backoff_rps = job->k.user_backoff_rps;
                    *run = job->asked;
                    return true;
                }
                if (job->count > 0) {
                    real_t rises[CHARGE_CAL_JOB_RUNS_MAX];
                    for (uint8_t i = 0; i < job->count; i++) {
                        rises[i] = job->runs[i].rise;
                    }
                    cal->backoff_unseen_gr = mean_of(rises, job->count);
                    cal->backoff_spread_gr = sd_of(rises, job->count);
                    cal->backoff_turns = job->k.user_backoff_turns;
                    cal->backoff_rps = job->k.user_backoff_rps;
                }
                job->phase = PHASE_DONE;
                continue;
            case PHASE_DONE:
            default:
                job->done = true;
                return false;
        }
    }
}


void charge_cal_job_reading(charge_cal_job_t *job, real_t t_s, real_t weight) {
    if (t_s <= REAL(0.0) && job->reading_count >= CHARGE_CAL_JOB_TURNING_MAX) {
        // A long turn: the newest reading takes the last one's place, so the stop's reading is kept and the watch
        // keeps its room. Only a prime's runs turn so long, and of them only the first and last and the watch are read
        job->times[job->reading_count - 1] = t_s;
        job->weights[job->reading_count - 1] = weight;
        return;
    }
    if (job->reading_count < CHARGE_CAL_JOB_READINGS_MAX) {
        job->times[job->reading_count] = t_s;
        job->weights[job->reading_count] = weight;
        job->reading_count++;
    }
}


/*
    How far the reading ran behind the powder turned out, summed over one run and its stop, in grain-seconds
    (staged.py's behind): over the powder poured it is how long that powder took, on average, from turned out to shown
*/
static real_t behind_of(const charge_cal_job_t *job, real_t run_s, real_t before, real_t end, real_t quiet) {
    real_t poured = end - before;
    real_t area = REAL(0.0);
    bool have = false;
    real_t t0 = REAL(0.0), shown0 = REAL(0.0);
    for (uint16_t i = 0; i < job->reading_count; i++) {
        if (job->times[i] > quiet) {
            continue;
        }
        real_t t1 = job->times[i] + run_s;
        real_t shown1 = min_f(job->weights[i], end) - before;
        if (have) {
            real_t out0 = poured * min_f(REAL(1.0), max_f(REAL(0.0), t0 / run_s));      // turned out evenly
            real_t out1 = poured * min_f(REAL(1.0), max_f(REAL(0.0), t1 / run_s));
            area += ((out0 - shown0) + (out1 - shown1)) / REAL(2.0) * (t1 - t0);
        }
        t0 = t1;
        shown0 = shown1;
        have = true;
    }
    return area;
}


void charge_cal_job_ran(charge_cal_job_t *job, real_t turns, real_t run_s) {
    const charge_cal_run_t *asked = &job->asked;
    // The run's readings: those at or before the stop are its own (the first the cup at rest), those after it its
    // watch's. The end is the median of the watch's last stretch
    uint16_t stop_at = 0;
    while (stop_at < job->reading_count && job->times[stop_at] <= REAL(0.0)) {
        stop_at++;
    }
    real_t tail[CHARGE_CAL_JOB_READINGS_MAX];
    uint16_t tail_count = 0;
    for (uint16_t i = stop_at; i < job->reading_count; i++) {
        if (job->times[i] >= asked->watch_s - REAL(CHARGE_CAL_END_WINDOW_S)) {
            tail[tail_count++] = job->weights[i];
        }
    }
    if (job->reading_count == 0 || stop_at == 0 || tail_count == 0) {
        return;     // no readings to make figures of: the caller ends the job
    }
    real_t before = job->weights[0];
    real_t at_stop = job->weights[stop_at - 1];
    real_t end = median_of(tail, tail_count);
    // When the reading first reaches where it ends: powder only adds, and a digit's flicker after it is not waited for
    real_t quiet = asked->watch_s;
    for (uint16_t i = stop_at; i < job->reading_count; i++) {
        if (job->weights[i] >= end - job->digit_gr / REAL(2.0)) {
            quiet = job->times[i];
            break;
        }
    }
    charge_cal_figures_t figures = {end - before, end - at_stop, quiet, turns >= REAL(0.0) ? turns : asked->speed_rps * run_s,
                                    REAL(0.0)};
    figures.behind = behind_of(job, run_s, before, end, quiet);

    switch ((phase_t) job->phase) {
        case PHASE_PRIME: {
            real_t gain = figures.gain;
            bool same = job->last_gain >= REAL(0.0) &&
                        fabs((double) (gain - job->last_gain)) <= (double) (REAL(CHARGE_CAL_PRIME_SAME_SHARE) * max_f(gain, job->last_gain));
            job->last_gain = gain;
            job->run = same ? CHARGE_CAL_PRIME_TRIES : (uint8_t) (job->run + 1);
            break;
        }
        case PHASE_PROBES:
            job->probe_quiet_most = max_f(job->probe_quiet_most, figures.quiet);
            job->run++;
            break;
        case PHASE_COARSE_FIGURE:
            job->coarse_gain_sum += figures.gain;
            job->coarse_turn_sum += figures.turns;
            if (job->coarse_yield_count < CHARGE_CAL_JOB_RUNS_MAX) {
                job->coarse_yields[job->coarse_yield_count++] = figures.gain / figures.turns;
            }
            job->run++;
            break;
        case PHASE_FINE_FIGURE:
            job->fine_gain_sum += figures.gain;
            job->fine_turn_sum += figures.turns;
            job->run++;
            break;
        case PHASE_COARSE_STOPS:
            if (job->coarse_yield_count < CHARGE_CAL_JOB_RUNS_MAX) {
                job->coarse_yields[job->coarse_yield_count++] = figures.gain / figures.turns;
            }
            job->runs[job->run++] = figures;
            break;
        case PHASE_BACKOFF:
            // The back-off's spill: what arrived after the turn back, from the last reading while it turned forward
            job->runs[job->run++] = figures;
            break;
        default:
            if (job->run < CHARGE_CAL_JOB_RUNS_MAX) {
                job->runs[job->run] = figures;
            }
            job->run++;
            break;
    }
}
