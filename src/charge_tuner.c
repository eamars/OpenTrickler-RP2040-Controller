/*
 * ADR-009 step 2: the auto-tuner (charge_tuner.h), ported from tools/charge_sim/step2_tuner.py's Tuner on the rule
 * the owner's objective passed with (gate 1, 19:52): a trial is kept when its time per good charge is lower than the
 * kept gains'. tests/step2_sim.py judges it against the Python window for window.
 */
#include "charge_tuner.h"

#include <math.h>
#include <string.h>

#define FIRST_FACTOR 1.5
#define LEAST_FACTOR 1.1
#define REST_WINDOWS 2


void charge_tuner_init(charge_tuner_t *tuner, double tolerance) {
    memset(tuner, 0, sizeof(*tuner));
    tuner->tolerance = tolerance;
    tuner->direction = 1;
    tuner->factor = FIRST_FACTOR;
}


// The middle time, the two middle ones' mean for an even count (statistics.median)
static double median_time(const double *times, int count) {
    double sorted[CHARGE_TUNER_POOL];
    for (int i = 0; i < count; i++) {
        int j = i;
        while (j > 0 && sorted[j - 1] > times[i]) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = times[i];
    }
    return count % 2 ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;
}


// Seconds a good charge: the median time over the share within the tolerance; infinite with none good
static double cost(const charge_tuner_t *tuner, const double *errors, const double *times, int count) {
    int good = 0;
    for (int i = 0; i < count; i++) {
        good += fabs(errors[i]) < tuner->tolerance;
    }
    double share = (double) good / count;
    return share > 0.0 ? median_time(times, count) / share : INFINITY;
}


// The window's throws are the kept kp's own
static void keep(charge_tuner_t *tuner, double kp) {
    tuner->has_base = true;
    tuner->base_kp = kp;
    memcpy(tuner->pool_errors, tuner->window_errors, sizeof(tuner->window_errors));
    memcpy(tuner->pool_times, tuner->window_times, sizeof(tuner->window_times));
    tuner->pool_count = CHARGE_TUNER_WINDOW;
    tuner->turned = false;
}


// The window's throws pooled with the kept kp's, the latest CHARGE_TUNER_POOL
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


static double mean_of(const double *values, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}


static double trial_kp(const charge_tuner_t *tuner) {
    return tuner->base_kp * pow(tuner->factor, tuner->direction);
}


static charge_tuner_step_t judge(charge_tuner_t *tuner, double kp, double *next_kp) {
    if (!tuner->has_base) {
        keep(tuner, kp);
        // The first trial goes the way the first window misses: over on average, a slower kp; else a quicker one
        tuner->direction = mean_of(tuner->window_errors, CHARGE_TUNER_WINDOW) > 0.0 ? -1 : 1;
        tuner->on_trial = true;
        *next_kp = trial_kp(tuner);
        return CHARGE_TUNER_FIRST;
    }
    if (!tuner->on_trial) {
        pool(tuner);
        if (tuner->resting > 0) {
            tuner->resting--;
            *next_kp = tuner->base_kp;
            return CHARGE_TUNER_RESTING;
        }
        tuner->on_trial = true;
        *next_kp = trial_kp(tuner);
        return CHARGE_TUNER_KEPT_AGAIN;
    }
    double trial_cost = cost(tuner, tuner->window_errors, tuner->window_times, CHARGE_TUNER_WINDOW);
    double base_cost = cost(tuner, tuner->pool_errors, tuner->pool_times, tuner->pool_count);
    if (trial_cost < base_cost) {
        keep(tuner, kp);
        tuner->factor = FIRST_FACTOR;
        *next_kp = trial_kp(tuner);
        return CHARGE_TUNER_KEPT;
    }
    if (!tuner->turned) {
        tuner->turned = true;
        tuner->direction = -tuner->direction;
        *next_kp = trial_kp(tuner);
        return CHARGE_TUNER_OTHER_WAY;
    }
    tuner->turned = false;
    tuner->factor = fmax(LEAST_FACTOR, sqrt(tuner->factor));
    tuner->resting = REST_WINDOWS;
    tuner->on_trial = false;
    *next_kp = tuner->base_kp;
    return CHARGE_TUNER_BOTH_WAYS;
}


charge_tuner_step_t charge_tuner_throw(charge_tuner_t *tuner, double kp, double error, double time_s, double *next_kp) {
    tuner->window_errors[tuner->window_count] = error;
    tuner->window_times[tuner->window_count] = time_s;
    tuner->window_count++;
    *next_kp = kp;
    if (tuner->window_count < CHARGE_TUNER_WINDOW) {
        return CHARGE_TUNER_THROWING;
    }
    tuner->window_count = 0;
    return judge(tuner, kp, next_kp);
}
