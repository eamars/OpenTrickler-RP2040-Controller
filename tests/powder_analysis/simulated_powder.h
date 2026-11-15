#ifndef SIMULATED_POWDER_H_
#define SIMULATED_POWDER_H_

// Simple powder model for the unit tests. It maps the gains of a throw to a final weight and
// durations in a monotonic way:
//   - overshoot grows with Kp and shrinks with Kd
//   - time shrinks with Kp and grows with Kd
// Below a Kp threshold the trickler "crawls" and does not overthrow at all. Above it the overshoot
// jumps to a base value and then grows with Kp (the coarse Kp search looks for this edge).

#include <stdint.h>
#include "powder_analysis_tuner.h"


typedef struct {
    float target_weight;
    float coarse_stop_threshold;
    float fine_stop_threshold;

    // Coarse trickler (overshoot in gr above the coarse stop point, time in s)
    float coarse_kp_threshold;      // no overthrow at or below this Kp
    float coarse_overshoot_base;    // overshoot right above the threshold (Kd = 0)
    float coarse_overshoot_slope;   // additional overshoot per unit Kp above the threshold
    float coarse_kd_damping;        // overshoot is divided by (1 + damping * Kd)
    float coarse_time_base;         // coarse time = time_base / Kp + slowdown * Kd
    float coarse_kd_slowdown;

    // Fine trickler (overshoot in gr above the point where the fine trickler stops, time in s)
    float fine_kp_threshold;
    float fine_overshoot_base;
    float fine_overshoot_slope;
    float fine_kd_damping;
    float fine_time_base;
    float fine_kd_slowdown;

    // Optional relative noise on the overshoots (0.2 = +/- 20 %), 0 = deterministic
    float noise_amplitude;
    // Optional relative noise on the coarse and fine times, 0 = deterministic
    float time_noise_amplitude;
    uint32_t noise_state;           // seed, advanced with every throw
} simulated_powder_t;


// Fill in a well-behaved powder: coarse Kp boundary at 0.05, fine Kp boundary at 0.7
void simulated_powder_init_default(simulated_powder_t *powder, float target_weight,
                                   float coarse_stop_threshold, float fine_stop_threshold);

// Run one throw with the given gains
void simulated_powder_throw(simulated_powder_t *powder,
                            const powder_analysis_trial_params_t *params,
                            powder_analysis_trial_result_t *result);


#endif  // SIMULATED_POWDER_H_
