#ifndef CHARGE_CALIBRATION_JOB_H_
#define CHARGE_CALIBRATION_JOB_H_

/*
    The calibration's measurements (ADR-009, gate 3): the steps of tools/charge_sim/staged.py's measure_on, in their
    order, as a state machine. No FreeRTOS, no motor, no scale, no allocation: the unit's job and the PC's check run
    the same code (the simulation loads it as a library, as it does charge_control.c).

    The caller asks for the next run (charge_cal_job_next): a tube, a speed, how long it turns and how long it is
    watched after its stop, and whether it turns back at its stop (the back-off's spill). It turns the tube, and gives
    every reading of the run with its time from the tube's stop (below zero while it turns, the first the cup at rest
    before it starts; charge_cal_job_reading), then the turns the tube made by its step count and how long it really
    turned (charge_cal_job_ran). When charge_cal_job_next says done, the measured part is in `cal`; the rules and the
    rehearsals are the caller's (charge_control_set_rules, then charge_control_place_stop).
*/

#include <stdbool.h>
#include <stdint.h>

#include "charge_control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHARGE_CAL_JOB_READINGS_MAX 1024        // a run's readings kept: 50 s at 20 a second (a size)
#define CHARGE_CAL_JOB_RUNS_MAX 24              // the most runs of one speed's stops or one move's size (a size)

typedef struct {
    uint8_t tube;                   // 0 coarse, 1 fine
    charge_control_real_t speed_rps;
    charge_control_real_t run_s;
    charge_control_real_t watch_s;
    bool backoff;                   // turned back at its stop by backoff_turns at backoff_rps, then held
    charge_control_real_t backoff_turns;
    charge_control_real_t backoff_rps;
} charge_cal_run_t;

// One run's figures, as staged.py's run_stop makes them
typedef struct {
    charge_control_real_t gain;     // the reading's rise from before the run to its end
    charge_control_real_t rise;     // from the stop to the end: what arrived after the stop
    charge_control_real_t quiet;    // when after the stop the reading first came within half a digit of its end
    charge_control_real_t turns;
    charge_control_real_t behind;   // grain-seconds the reading ran behind the powder turned out
} charge_cal_figures_t;

typedef struct {
    // In
    charge_control_real_t digit_gr;
    charge_control_real_t interval_s;
    charge_control_constants_t k;   // the user's limits and back-off (k's coarse_least_rps ... user_backoff_rps)
    // Out: the measured part, when done
    charge_control_calibration_t cal;
    bool done;
    // The steps
    uint8_t phase;
    uint8_t tube;                   // the prime's tube
    uint8_t index;                  // the step's entry (a ladder's stop, a move's size)
    uint8_t run;                    // the run within the entry
    uint8_t count;                  // the runs the entry asks
    charge_control_real_t speed;    // the entry's speed
    charge_control_real_t run_s;
    charge_control_real_t last_gain;            // the prime's last run, or below zero
    charge_cal_run_t asked;
    // The run in hand: its readings
    uint16_t reading_count;
    charge_control_real_t times[CHARGE_CAL_JOB_READINGS_MAX];
    charge_control_real_t weights[CHARGE_CAL_JOB_READINGS_MAX];
    // The entry's runs, made into figures
    charge_cal_figures_t runs[CHARGE_CAL_JOB_RUNS_MAX];
    // Across the steps
    charge_control_real_t probe_quiet_most;
    charge_control_real_t coarse_gain_sum, coarse_turn_sum, fine_gain_sum, fine_turn_sum;
    charge_control_real_t coarse_yields[CHARGE_CAL_JOB_RUNS_MAX];
    uint8_t coarse_yield_count;
} charge_cal_job_t;

// A calibration's measurements begun: the scale's digit and usual interval, and the user's limits in k
void charge_cal_job_begin(charge_cal_job_t *job, charge_control_real_t digit_gr, charge_control_real_t interval_s,
                          const charge_control_constants_t *k);
// The next run to make, into *run; false when the measurements are done (job->done, job->cal)
bool charge_cal_job_next(charge_cal_job_t *job, charge_cal_run_t *run);
// A reading of the run in hand, its time from the tube's stop in seconds (below zero while it turns)
void charge_cal_job_reading(charge_cal_job_t *job, charge_control_real_t t_s, charge_control_real_t weight);
// The run in hand made: the tube's turns by its count (below zero: none counted, its speed times run_s is taken)
// and how long it really turned
void charge_cal_job_ran(charge_cal_job_t *job, charge_control_real_t turns, charge_control_real_t run_s);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CALIBRATION_JOB_H_
