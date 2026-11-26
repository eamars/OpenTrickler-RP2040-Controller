/*
 * ADR-009 step 2: the auto-tuner (charge_tuner.h), ported from tools/charge_sim/step2_tuner.py's Tuner with the axes
 * "kp+kd" and the share modelled, on the rule the owner's objective passed with (gate 1, 19:52): a trial is kept when
 * its time per good charge is lower than the kept gains'. tests/step2_sim.py judges it against the Python window for
 * window.
 */
#include "charge_tuner.h"

#include <math.h>
#include <string.h>

#define FIRST_FACTOR 1.5
#define LEAST_FACTOR 1.1
#define REST_WINDOWS 2
#define AXES 2
#define NO_MOVE 0.01                    // a braking step smaller than this does not move the gains


void charge_tuner_init(charge_tuner_t *tuner, double tolerance) {
    memset(tuner, 0, sizeof(*tuner));
    tuner->tolerance = tolerance;
    tuner->direction = 1;
    tuner->factor = FIRST_FACTOR;
}


// The values in order, into sorted (insertion: 75 at most)
static void sorted_copy(const double *values, int count, double *sorted) {
    for (int i = 0; i < count; i++) {
        int j = i;
        while (j > 0 && sorted[j - 1] > values[i]) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = values[i];
    }
}


// The middle value of sorted ones, the two middle ones' mean for an even count (statistics.median)
static double median_sorted(const double *sorted, int count) {
    return count % 2 ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;
}


static double mean_of(const double *values, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}


// erf by Abramowitz and Stegun 7.1.26 (an error under 1.5e-7), as step2_tuner.erf_approx: libm's erf and its exp are
// placed in RAM on the Pico W, 2.6 KB it has not got (2026-11-26); exp is the SDK's, on the chip's ROM
static double erf_approx(double x) {
    double sign = x < 0.0 ? -1.0 : 1.0;
    x = fabs(x);
    double t = 1.0 / (1.0 + 0.3275911 * x);
    double poly = t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429))));
    return sign * (1.0 - poly * exp(-x * x));
}


static double normal_cdf(double x, double mu, double sigma) {
    return 0.5 * (1.0 + erf_approx((x - mu) / (sigma * sqrt(2.0))));
}


// The share within the tolerance of a normal spread with the throws' median and the half-width of their middle 68%
// (step2_tuner.modelled)
static double modelled_share(const charge_tuner_t *tuner, const double *errors, int count) {
    double sorted[CHARGE_TUNER_POOL];
    sorted_copy(errors, count, sorted);
    double centre = median_sorted(sorted, count);
    double width = (sorted[(int) (0.84 * (count - 1))] - sorted[(int) (0.16 * (count - 1))]) / 2.0;
    if (width <= 0.0) {
        return fabs(centre) < tuner->tolerance ? 1.0 : 0.0;
    }
    return normal_cdf(tuner->tolerance, centre, width) - normal_cdf(-tuner->tolerance, centre, width);
}


// Seconds a good charge: the median time over the share within the tolerance; infinite with none
static double cost(const charge_tuner_t *tuner, const double *errors, const double *times, int count) {
    double share = modelled_share(tuner, errors, count);
    double sorted[CHARGE_TUNER_POOL];
    sorted_copy(times, count, sorted);
    return share > 0.0 ? median_sorted(sorted, count) / share : INFINITY;
}


// The window's throws are the kept gains' own
static void keep(charge_tuner_t *tuner, double kp, double kd, double coarse_kp) {
    tuner->has_base = true;
    tuner->base_kp = kp;
    tuner->base_kd = kd;
    tuner->base_coarse_kp = coarse_kp;
    memcpy(tuner->pool_errors, tuner->window_errors, sizeof(tuner->window_errors));
    memcpy(tuner->pool_times, tuner->window_times, sizeof(tuner->window_times));
    tuner->pool_count = CHARGE_TUNER_WINDOW;
    tuner->tried = 0;
}


// The window's throws pooled with the kept gains', the latest CHARGE_TUNER_POOL
static void pool(charge_tuner_t *tuner) {
    for (int i = 0; i < CHARGE_TUNER_WINDOW; i++) {
        if (tuner->pool_count == CHARGE_TUNER_POOL) {
            memmove(tuner->pool_errors, &tuner->pool_errors[1], (CHARGE_TUNER_POOL - 1) * sizeof(double));
            memmove(tuner->pool_times, &tuner->pool_times[1], (CHARGE_TUNER_POOL - 1) * sizeof(double));
            tuner->pool_count--;
        }
        tuner->pool_errors[tuner->pool_count] = tuner->window_errors[i];
        tuner->pool_times[tuner->pool_count] = tuner->window_times[i];
        tuner->pool_count++;
    }
}


// The way an axis is tried first: after throws over on average, a lower gain and more braking
static int missing_way(const charge_tuner_t *tuner, const double *errors, int count) {
    bool over = mean_of(errors, count) > 0.0;
    if (tuner->axis == CHARGE_TUNER_GAIN) {
        return over ? -1 : 1;
    }
    return over ? 1 : -1;
}


static double braking(double kd, double k) {
    return 1.0 + kd * k / 1000.0;
}


// The trial's gains from the kept ones: false when the step would not move them
static bool trial(const charge_tuner_t *tuner, double *kp, double *kd) {
    double step = pow(tuner->factor, tuner->direction);
    if (tuner->axis == CHARGE_TUNER_GAIN) {
        *kp = tuner->base_kp * step;
        *kd = tuner->base_kd;
        return true;
    }
    if (tuner->k <= 0.0) {
        return false;
    }
    double before = braking(tuner->base_kd, tuner->k);
    double after = fmax(1.0, before * step);
    if (fabs(after - before) < NO_MOVE) {
        return false;
    }
    *kp = tuner->base_kp * after / before;
    *kd = (after - 1.0) * 1000.0 / tuner->k;
    return true;
}


// The trial no cheaper: the other way, then the other axis; every way put back, the step shrinks and the kept gains
// rest. A trial that would not move the gains counts as put back unthrown
static charge_tuner_step_t put_back(charge_tuner_t *tuner, double *next_kp, double *next_kd) {
    while (true) {
        charge_tuner_step_t step;
        tuner->tried++;
        if (tuner->tried % 2 == 1) {
            tuner->direction = -tuner->direction;
            step = CHARGE_TUNER_OTHER_WAY;
        }
        else if (tuner->tried < 2 * AXES) {
            tuner->axis = (tuner->axis + 1) % AXES;
            tuner->direction = missing_way(tuner, tuner->pool_errors, tuner->pool_count);
            step = CHARGE_TUNER_OTHER_AXIS;
        }
        else {
            tuner->tried = 0;
            tuner->factor = fmax(LEAST_FACTOR, sqrt(tuner->factor));
            tuner->resting = REST_WINDOWS;
            tuner->axis = CHARGE_TUNER_GAIN;
            tuner->direction = missing_way(tuner, tuner->pool_errors, tuner->pool_count);
            tuner->on_trial = false;
            *next_kp = tuner->base_kp;
            *next_kd = tuner->base_kd;
            return CHARGE_TUNER_EVERY_WAY;
        }
        if (trial(tuner, next_kp, next_kd) && (*next_kp != tuner->base_kp || *next_kd != tuner->base_kd)) {
            tuner->on_trial = true;
            return step;
        }
    }
}


// The kept gains put right, true when they need it: the steady gain into range by the weight a turn and the lag, and
// the coarse kp down to CHARGE_TUNER_COARSE_GAIN_MOST by step 1's coarse weight a turn and the
// lag, into coarse_put_right (step2_tuner.Tuner.put_right)
static bool put_right(charge_tuner_t *tuner, double *kp, double *kd) {
    *kp = tuner->base_kp;
    *kd = tuner->base_kd;
    if (tuner->k > 0.0 && tuner->lag > 0.0) {
        double hold = braking(tuner->base_kd, tuner->k);
        double gain = tuner->base_kp / hold * tuner->k * tuner->lag;
        if (!(CHARGE_TUNER_GAIN_LOW <= gain && gain <= CHARGE_TUNER_GAIN_HIGH)) {
            *kp = CHARGE_TUNER_GAIN_AIM * hold / (tuner->k * tuner->lag);
        }
    }
    tuner->coarse_put_right = tuner->base_coarse_kp;
    if (tuner->coarse_k > 0.0 && tuner->lag > 0.0 &&
        tuner->base_coarse_kp * tuner->coarse_k * tuner->lag > CHARGE_TUNER_COARSE_GAIN_MOST) {
        tuner->coarse_put_right = CHARGE_TUNER_COARSE_GAIN_MOST / (tuner->coarse_k * tuner->lag);
    }
    return *kp != tuner->base_kp || tuner->coarse_put_right != tuner->base_coarse_kp;
}


// The kept gains again, or a trial from them; a trial that would not move them is put back unthrown
static charge_tuner_step_t next_trial(charge_tuner_t *tuner, charge_tuner_step_t step, double *next_kp,
                                      double *next_kd) {
    if (trial(tuner, next_kp, next_kd)) {
        tuner->on_trial = true;
        return step;
    }
    return put_back(tuner, next_kp, next_kd);
}


static charge_tuner_step_t judge(charge_tuner_t *tuner, double kp, double kd, double coarse_kp,
                                 double *next_kp, double *next_kd) {
    if (!tuner->has_base) {
        keep(tuner, kp, kd, coarse_kp);
        tuner->direction = missing_way(tuner, tuner->window_errors, CHARGE_TUNER_WINDOW);
        if (put_right(tuner, next_kp, next_kd)) {
            tuner->putting_right = true;
            tuner->on_trial = true;
            return CHARGE_TUNER_FIRST;
        }
        return next_trial(tuner, CHARGE_TUNER_FIRST, next_kp, next_kd);
    }
    if (!tuner->on_trial) {
        pool(tuner);
        if (tuner->resting > 0) {
            tuner->resting--;
            *next_kp = tuner->base_kp;
            *next_kd = tuner->base_kd;
            return CHARGE_TUNER_RESTING;
        }
        return next_trial(tuner, CHARGE_TUNER_KEPT_AGAIN, next_kp, next_kd);
    }
    double trial_cost = cost(tuner, tuner->window_errors, tuner->window_times, CHARGE_TUNER_WINDOW);
    double base_cost = cost(tuner, tuner->pool_errors, tuner->pool_times, tuner->pool_count);
    if (tuner->putting_right) {
        tuner->putting_right = false;
        if (trial_cost < base_cost) {
            keep(tuner, kp, kd, coarse_kp);
            tuner->factor = FIRST_FACTOR;
            tuner->direction = missing_way(tuner, tuner->window_errors, CHARGE_TUNER_WINDOW);
            return next_trial(tuner, CHARGE_TUNER_KEPT, next_kp, next_kd);
        }
        return next_trial(tuner, CHARGE_TUNER_AS_THEY_WERE, next_kp, next_kd);
    }
    if (trial_cost < base_cost) {
        keep(tuner, kp, kd, coarse_kp);
        tuner->factor = FIRST_FACTOR;
        return next_trial(tuner, CHARGE_TUNER_KEPT, next_kp, next_kd);
    }
    return put_back(tuner, next_kp, next_kd);
}


charge_tuner_step_t charge_tuner_throw(charge_tuner_t *tuner, double kp, double kd, double coarse_kp,
                                       double k, double coarse_k, double lag, double error, double time_s,
                                       double *next_kp, double *next_kd, double *next_coarse_kp) {
    if (k > 0.0) {
        tuner->k = k;
    }
    if (coarse_k > 0.0) {
        tuner->coarse_k = coarse_k;
    }
    if (lag > 0.0) {
        tuner->lag = lag;
    }
    tuner->window_errors[tuner->window_count] = error;
    tuner->window_times[tuner->window_count] = time_s;
    tuner->window_count++;
    *next_kp = kp;
    *next_kd = kd;
    *next_coarse_kp = coarse_kp;
    if (tuner->window_count < CHARGE_TUNER_WINDOW) {
        return CHARGE_TUNER_THROWING;
    }
    tuner->window_count = 0;
    charge_tuner_step_t step = judge(tuner, kp, kd, coarse_kp, next_kp, next_kd);
    // The coarse kp is moved by the gains put right alone: every other window is thrown on the kept one
    *next_coarse_kp = tuner->putting_right ? tuner->coarse_put_right : tuner->base_coarse_kp;
    return step;
}
