#ifndef CHARGE_TUNER_H_
#define CHARGE_TUNER_H_
/*
 * ADR-009 step 2: the auto-tuner, new_pid = tuner(old_pid, charge_data), as tools/charge_sim/step2_tuner.py runs it
 * on the twin (axes "kp+kd", the share modelled), in C with nothing of the RTOS in it; tests/step2_sim.py judges it
 * against the Python on the same windows.
 *
 * After each throw, per powder: the throw's final weight less the target and its time to the stop. Each window of
 * CHARGE_TUNER_WINDOW throws is judged by the owner's objective, its time per good charge (its median time over its
 * share within the tolerance), against the kept gains' throws (the latest CHARGE_TUNER_POOL). The share is reckoned
 * from every throw's weight: a normal spread with the throws' median and the half-width of their middle 68%, so a
 * window of 25 decides as a count of its good throws cannot (2026-11-26).
 *
 * It moves the fine tube's gains on two axes, by a factor (the owner, 2026-11-26: "consider Kd as well"):
 * - the steady gain: kp moved, kd held;
 * - the braking, 1 + kd x k / 1000 (k the weight a turn; the D term is per millisecond): kd moved, and kp with it so
 *   the steady gain holds. It brakes the fine tube on what the forecast does not hold, the coarse tube's powder
 *   landing. Never under 1: kd never negative.
 * A trial cheaper than the kept gains is kept and the same way tried again at the full step; one no cheaper is put
 * back and the other way tried at once, then the other axis; every way put back, the step's logarithm halves (down
 * to 1.1) and the kept gains are thrown again a while. The first way on an axis is the one the throws miss by: over
 * on average, a lower gain and more braking. Ratios only, so grams and grains are alike. Everything is in RAM: a
 * start of the unit begins afresh from the profile's gains. What is stored into the profile is the kept gains alone.
 */
#include <stdbool.h>

#define CHARGE_TUNER_WINDOW 25
#define CHARGE_TUNER_POOL 75

typedef enum {
    CHARGE_TUNER_THROWING = 0,          // the window is not full yet
    CHARGE_TUNER_FIRST,                 // the first window: its gains kept, a trial next
    CHARGE_TUNER_RESTING,               // the kept gains thrown again, every way having been put back
    CHARGE_TUNER_KEPT_AGAIN,            // the kept gains' window pooled, a trial next
    CHARGE_TUNER_KEPT,                  // the trial was cheaper: kept, the same way tried again
    CHARGE_TUNER_OTHER_WAY,             // the trial no cheaper: put back, the other way tried
    CHARGE_TUNER_EVERY_WAY,             // every way put back: the step smaller, the kept gains thrown again
    CHARGE_TUNER_OTHER_AXIS,            // both ways of an axis put back: the other axis tried
} charge_tuner_step_t;

typedef enum {
    CHARGE_TUNER_GAIN = 0,
    CHARGE_TUNER_BRAKING = 1,
} charge_tuner_axis_t;

typedef struct {
    double tolerance;                   // the user's: the fine threshold
    double window_errors[CHARGE_TUNER_WINDOW];
    double window_times[CHARGE_TUNER_WINDOW];
    int window_count;
    bool has_base;
    double base_kp;                     // the kept gains: the ones stored into the profile
    double base_kd;
    double pool_errors[CHARGE_TUNER_POOL];      // the throws thrown on them, oldest first
    double pool_times[CHARGE_TUNER_POOL];
    int pool_count;
    bool on_trial;                      // the window being thrown is a trial's
    int axis;                           // charge_tuner_axis_t
    int direction;
    double factor;
    int tried;                          // ways put back since the kept gains were kept or the step changed
    int resting;
    double k;                           // the weight a turn the braking is reckoned with; 0, none known
} charge_tuner_t;

#ifdef __cplusplus
extern "C" {
#endif

void charge_tuner_init(charge_tuner_t *tuner, double tolerance);

/*
 * A throw thrown on kp and kd: its final weight less the target, and its time to the stop; k, the weight a turn the
 * forecast carries now (0 when none). When it closes a window, the gains the next window is thrown on go into
 * next_kp and next_kd and the step taken is returned; else CHARGE_TUNER_THROWING, and they are kp and kd
 */
charge_tuner_step_t charge_tuner_throw(charge_tuner_t *tuner, double kp, double kd, double k, double error,
                                       double time_s, double *next_kp, double *next_kd);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_TUNER_H_
