#ifndef CHARGE_TUNER_H_
#define CHARGE_TUNER_H_
/*
 * ADR-009 step 2: the auto-tuner, new_pid = tuner(old_pid, charge_data), as tools/charge_sim/step2_tuner.py runs it
 * on the twin (axes "kp+kd", the share counted), in C with nothing of the RTOS in it; tests/step2_sim.py judges it
 * against the Python on the same windows.
 *
 * After each throw, per powder: the throw's final weight less the target and its time to the stop. Each window of
 * CHARGE_TUNER_WINDOW throws is judged by the owner's objective, the tolerance first, then quicker (2026-11-26,
 * replacing the time per good charge): its share within the tolerance against the kept gains' throws (the latest
 * CHARGE_TUNER_POOL), and between shares alike its median time. The share is counted, as the unit judges each throw:
 * modelled from the throws' spread it snapped to the scale's 0.02 grid on the bench (2026-11-26).
 *
 * It moves the fine tube's gains on two axes, by a factor (the owner, 2026-11-26: "consider Kd as well"):
 * - the steady gain: kp moved, kd held;
 * - the braking, 1 + kd x k / 1000 (k the weight a turn; the D term is per millisecond): kd moved, and kp with it so
 *   the steady gain holds. It brakes the fine tube on what the forecast does not hold, the coarse tube's powder
 *   landing. Never under 1: kd never negative.
 * A trial better than the kept gains is kept and the same way tried again; one no better is put back and the other
 * way tried at once, then the other axis; every way put back, nothing beats the kept gains and they are held (the
 * owner, 2026-11-26), until the held gains' latest CHARGE_TUNER_HOLD_RECENT throws pass less than they did by more than
 * their noise, when the search begins again from them. The first way on an axis is the one the throws miss by: over
 * on average, a lower gain and more braking. Ratios only, so grams and grains are alike. Before the search, the first
 * window's gains are put right where they need it, as one trial kept only if cheaper (2026-11-26): the steady gain kp /
 * braking times step 1's weight a turn and middle lag is a number with no unit (the bench's 1.3, the owner's 0.5), so
 * out of CHARGE_TUNER_GAIN_LOW to CHARGE_TUNER_GAIN_HIGH it is brought to CHARGE_TUNER_GAIN_AIM: grains' gains on a scale
 * in grams (0.08), a gram's in grains (20), an absurd kp. The integral term is never the tuner's (the owner,
 * 2026-11-26: "The tuner should never intentionally touch Ki"): an integral the user set is met by kp and kd, and ki is
 * not in the tuner at all. The same window puts the coarse kp right too
 * (the owner, 2026-11-26: "use and tune PID ... tune under the existing framework"): kp x step 1's coarse weight a turn
 * x the middle lag, a number with no unit, above CHARGE_TUNER_COARSE_GAIN_MOST is brought down to it; never up (tuned
 * up on the twin it gained no time from the shipped gains, and a wild one took a 30 gr charge 14.5 gr past). The coarse
 * kp is moved nowhere else. Everything is in RAM: a
 * start of the unit begins afresh from the profile's gains. What is stored into the profile is the kept gains alone.
 */
#include <stdbool.h>

#define CHARGE_TUNER_WINDOW 25
#define CHARGE_TUNER_POOL 75
#define CHARGE_TUNER_GAIN_LOW 0.2
#define CHARGE_TUNER_GAIN_HIGH 5.0
#define CHARGE_TUNER_GAIN_AIM 1.0
#define CHARGE_TUNER_COARSE_GAIN_MOST 0.4
#define CHARGE_TUNER_SHARE_Z 0.5          // a share better by this many of the two shares' noise (step2_tuner.SHARE_Z)
#define CHARGE_TUNER_TIME_MOST_S 10.0     // a window's median time past it loses (the owner, 2026-11-26; to test first)
#define CHARGE_TUNER_FAR_SHARE 0.5        // both shares under it: judged by the mean miss (the owner, 2026-11-26)
#define CHARGE_TUNER_HOLD_RECENT 50       // the held gains' latest throws a hold is judged on

typedef enum {
    CHARGE_TUNER_THROWING = 0,          // the window is not full yet
    CHARGE_TUNER_FIRST,                 // the first window: its gains kept, a trial next
    CHARGE_TUNER_HOLDING,               // the kept gains held: nothing beat them, and they still pass as they did
    CHARGE_TUNER_KEPT_AGAIN,            // the kept gains' window pooled, a trial next
    CHARGE_TUNER_KEPT,                  // the trial was cheaper: kept, the same way tried again
    CHARGE_TUNER_OTHER_WAY,             // the trial no cheaper: put back, the other way tried
    CHARGE_TUNER_HOLD,                  // every way put back: nothing beats the kept gains, held from now on
    CHARGE_TUNER_OTHER_AXIS,            // both ways of an axis put back: the other axis tried
    CHARGE_TUNER_AS_THEY_WERE,          // the gains put right no cheaper: put back as they were
    CHARGE_TUNER_RESUMED,               // the held gains pass less than they did: the search begins again from them
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
    bool putting_right;                 // that trial is the first window's gains put right
    int axis;                           // charge_tuner_axis_t
    int direction;
    double factor;
    int tried;                          // ways put back since the kept gains were kept
    bool holding;                       // every way put back: the kept gains held
    double held_share;                  // the kept gains' share when the hold began
    double recent[CHARGE_TUNER_HOLD_RECENT];    // the held gains' latest throws, oldest first
    int recent_count;
    double k;                           // the weight a turn the braking is reckoned with; 0, none known
    double lag;                         // step 1's middle lag, s, for the gains put right; 0, none known
    double base_coarse_kp;              // the kept coarse kp
    double coarse_put_right;            // the coarse kp the gains put right are thrown with
    double coarse_k;                    // step 1's coarse weight a turn; 0, none known
} charge_tuner_t;

#ifdef __cplusplus
extern "C" {
#endif

void charge_tuner_init(charge_tuner_t *tuner, double tolerance);

/*
 * A throw thrown on the fine kp and kd and the coarse kp: its final weight less the target, and its time to the
 * stop; k, the weight a turn the forecast carries now, coarse_k, step 1's coarse weight a turn, and lag, step 1's
 * middle lag (0 when none). When it closes a window, the gains the next window is thrown on go into next_kp, next_kd and
 * next_coarse_kp and the step taken is returned; else CHARGE_TUNER_THROWING, and they are the gains thrown
 */
charge_tuner_step_t charge_tuner_throw(charge_tuner_t *tuner, double kp, double kd, double coarse_kp,
                                       double k, double coarse_k, double lag, double error, double time_s,
                                       double *next_kp, double *next_kd, double *next_coarse_kp);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_TUNER_H_
