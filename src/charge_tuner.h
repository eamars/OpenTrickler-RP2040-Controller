#ifndef CHARGE_TUNER_H_
#define CHARGE_TUNER_H_
/*
 * ADR-009 step 2: the auto-tuner, new_pid = tuner(old_pid, charge_data), as tools/charge_sim/step2_tuner.py runs it
 * on the twin (gate 1), in C with nothing of the RTOS in it; tests/step2_sim.py judges it against the Python on the
 * same windows (gate 2).
 *
 * After each throw, per powder: the throw's final weight less the target and its time to the stop. Each window of
 * CHARGE_TUNER_WINDOW throws is judged by the owner's objective, its time per good charge (its median time over its
 * share within the tolerance), against the kept gains' throws (the latest CHARGE_TUNER_POOL). It moves the fine kp
 * alone, by a factor, its first trial the way the first window misses (over on average, slower; else quicker): a
 * trial cheaper than the kept gains is kept and the same way tried again at the full step; one
 * no cheaper is put back and the other way tried at once; both put back, the step's logarithm halves (down to
 * 1.1) and the kept gains are thrown again a while. Ratios only, so grams and grains are alike. Everything is in RAM: a
 * start of the unit begins afresh from the profile's kp. What is stored into the profile is the kept kp alone.
 */
#include <stdbool.h>

#define CHARGE_TUNER_WINDOW 25
#define CHARGE_TUNER_POOL 75

typedef enum {
    CHARGE_TUNER_THROWING = 0,          // the window is not full yet
    CHARGE_TUNER_FIRST,                 // the first window: its kp kept, a trial next
    CHARGE_TUNER_RESTING,               // the kept kp thrown again, both ways having been put back
    CHARGE_TUNER_KEPT_AGAIN,            // the kept kp's window pooled, a trial next
    CHARGE_TUNER_KEPT,                  // the trial was cheaper: kept, the same way tried again
    CHARGE_TUNER_OTHER_WAY,             // the trial no cheaper: put back, the other way tried
    CHARGE_TUNER_BOTH_WAYS,             // both ways put back: the step smaller, the kept kp thrown again
} charge_tuner_step_t;

typedef struct {
    double tolerance;                   // the user's: the fine threshold
    double window_errors[CHARGE_TUNER_WINDOW];
    double window_times[CHARGE_TUNER_WINDOW];
    int window_count;
    bool has_base;
    double base_kp;                     // the kept kp: the one stored into the profile
    double pool_errors[CHARGE_TUNER_POOL];      // the throws thrown on it, oldest first
    double pool_times[CHARGE_TUNER_POOL];
    int pool_count;
    bool on_trial;                      // the window being thrown is a trial's
    int direction;
    double factor;
    bool turned;                        // the other way already tried from this base
    int resting;
} charge_tuner_t;

#ifdef __cplusplus
extern "C" {
#endif

void charge_tuner_init(charge_tuner_t *tuner, double tolerance);

/*
 * A throw thrown on kp: its final weight less the target, and its time to the stop. When it closes a window, the kp
 * the next window is thrown on goes into next_kp and the step taken is returned; else CHARGE_TUNER_THROWING, and
 * next_kp is kp
 */
charge_tuner_step_t charge_tuner_throw(charge_tuner_t *tuner, double kp, double error, double time_s, double *next_kp);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_TUNER_H_
