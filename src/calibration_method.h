#ifndef CALIBRATION_METHOD_H_
#define CALIBRATION_METHOD_H_
/*
 * ADR-009 step 1: the calibration's method, its scale part and a tube's speeds part, as tools/charge_sim/
 * calibration.py runs them on the PC (gate 1), in C with nothing of the RTOS, the motors or the scale in it. It turns
 * the tubes and reads the scale through a bench: on the unit the job's (calibration.c), on the PC the simulation's
 * (tests/calibration_sim.py), so the firmware's own method is run against the simulation as well as on the unit.
 *
 * No retries and no pass criteria: a part that cannot measure an item says so, and the item keeps its value.
 */
#include <stdbool.h>
#include <stdint.h>

#define CALIBRATION_COARSE 0
#define CALIBRATION_FINE 1
#define CALIBRATION_LAG_POINTS 5        // as the scale's settings keep it (SCALE_LAG_POINTS)
#define CALIBRATION_DURING_MAX 128      // the readings of a run kept: a lag's runs, 2 s at most; the longer runs
                                        // need only their first
#define CALIBRATION_AFTER_MAX 256       // of a watch: 12 s of the A&D's 20 a second
#define CALIBRATION_FAILURE_LEN 72

// A reading: its time in seconds, from the start of a run or from a stop, and the weight
typedef struct {
    float t_s;
    float weight;
} calibration_sample_t;

/*
 * The bench the method turns and reads, and the room it keeps a run's readings in: `during` and `after` of
 * CALIBRATION_DURING_MAX and CALIBRATION_AFTER_MAX readings, the job's from its heap while a part runs (the Pico W's
 * RAM has no 3 KB to spare for good). Each call is the job's or the simulation's; ctx is theirs.
 * - set_speed: a tube's speed in rev/s, 0 to stop it.
 * - watch: let `seconds` pass, the tubes as they are, each reading the scale gives meanwhile into `out` (up to `room`,
 *   later ones dropped), its time from the call. The count, or -1 when the calibration is stopped.
 * - cup_full: the cup holds `held` and a run would pour up to `most` more, passing its capacity: the calibration
 *   pauses for the cup to be emptied and put back. True to go on, false when stopped.
 * - report: how far the part is, 0 to 100, and what it does, in words for the screen ("Fine tube 0.12 rev/s").
 */
typedef struct {
    void *ctx;
    calibration_sample_t *during;
    calibration_sample_t *after;
    void (*set_speed)(void *ctx, int tube, float rps);
    int (*watch)(void *ctx, float seconds, calibration_sample_t *out, int room);
    bool (*cup_full)(void *ctx, float held, float most);
    void (*report)(void *ctx, int percent, const char *line);
} calibration_bench_t;

typedef enum {
    CALIBRATION_DONE = 0,       // the part ran to its end: its records, and failures where an item was not measured
    CALIBRATION_STOPPED,        // stopped by the person, or at a fault
} calibration_outcome_t;

// The scale part's records (items 3 and 4); NAN where not measured
typedef struct {
    float settle_s;
    int lag_count;              // the flows measured, slowest first
    float lag_flow_per_s[CALIBRATION_LAG_POINTS];
    float lag_s[CALIBRATION_LAG_POINTS];
    float seconds;
    float poured;
    char failure[CALIBRATION_FAILURE_LEN];      // the first item not measured, in words; empty when none
} calibration_scale_record_t;

// A tube's speeds part's record (item 5); NAN where not measured
typedef struct {
    float least_rps;
    float stream_per_turn;      // its weight a turn where it streams (the prime's)
    float seconds;
    float poured;
    char failure[CALIBRATION_FAILURE_LEN];
} calibration_speeds_record_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The cup's capacity and the powder in it now, as the cup's part and the scale give them: no run passes the
 * capacity. NAN capacity: none known, and a default is taken (CALIBRATION_DEFAULT_CAPACITY).
 */
void calibration_cup(float capacity, float held_now);

// The scale part: its settle time and its lag, from the fine tube's bursts and both tubes' steady runs
calibration_outcome_t calibration_scale_part(const calibration_bench_t *bench, calibration_scale_record_t *record);

/*
 * A tube's speeds part: its least speed, twice the slowest speed at which it pours. least_rps and most_rps are its
 * motor's own limits. scale: the scale part's records, or NULL for the defaults (they set how long a stop is watched)
 */
calibration_outcome_t calibration_speeds_part(const calibration_bench_t *bench, int tube, float least_rps,
                                              float most_rps, const calibration_scale_record_t *scale,
                                              calibration_speeds_record_t *record);

// How long a stop is watched on a scale with these records (NULL, or not both measured: the default)
float calibration_watch_after_stop(const calibration_scale_record_t *scale);

// The reading at rest after a stop, when it first moved (NAN when never) and when it came to rest (0 when it never
// moved), from the readings of a watch of watch_s: a flicker of one step from the rest is no move
void calibration_settled(const calibration_sample_t *after, int count, float watch_s, float *rest, float *first,
                         float *settle);

#ifdef __cplusplus
}
#endif

#endif  // CALIBRATION_METHOD_H_
