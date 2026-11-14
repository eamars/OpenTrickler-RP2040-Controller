#include <string.h>
#include "simulated_powder.h"


void simulated_powder_init_default(simulated_powder_t *powder, float target_weight,
                                   float coarse_stop_threshold, float fine_stop_threshold) {
    memset(powder, 0x0, sizeof(*powder));

    powder->target_weight = target_weight;
    powder->coarse_stop_threshold = coarse_stop_threshold;
    powder->fine_stop_threshold = fine_stop_threshold;

    // Coarse: at Kp 0.05 the overshoot jumps to 1.6 gr, Kd 0.1 brings it down to 0.8 gr
    powder->coarse_kp_threshold = 0.05f;
    powder->coarse_overshoot_base = 1.6f;
    powder->coarse_overshoot_slope = 10.0f;
    powder->coarse_kd_damping = 10.0f;
    powder->coarse_time_base = 0.2f;
    powder->coarse_kd_slowdown = 5.0f;

    // Fine: at Kp 0.7 the overshoot jumps to 0.065 gr (tolerance is 2 * 0.02 gr), Kd ~12 fixes it
    powder->fine_kp_threshold = 0.7f;
    powder->fine_overshoot_base = 0.065f;
    powder->fine_overshoot_slope = 0.05f;
    powder->fine_kd_damping = 0.05f;
    powder->fine_time_base = 2.0f;
    powder->fine_kd_slowdown = 0.1f;

    powder->noise_amplitude = 0.0f;
    powder->noise_state = 12345;
}


// Deterministic noise in [-1, 1] (linear congruential generator)
static float next_noise(simulated_powder_t *powder) {
    powder->noise_state = powder->noise_state * 1664525u + 1013904223u;
    return ((powder->noise_state >> 8) % 20001) / 10000.0f - 1.0f;
}


static float overshoot(float kp, float kd, float kp_threshold, float base, float slope, float damping) {
    if (kp <= kp_threshold) {
        return 0.0f;
    }
    return (base + slope * (kp - kp_threshold)) / (1.0f + damping * kd);
}


static float duration(float kp, float kd, float time_base, float slowdown) {
    if (kp <= 0.0f) {
        return 1000.0f;     // no Kp, the trickler crawls forever
    }
    return time_base / kp + slowdown * kd;
}


void simulated_powder_throw(simulated_powder_t *powder,
                            const powder_analysis_trial_params_t *params,
                            powder_analysis_trial_result_t *result) {
    memset(result, 0x0, sizeof(*result));

    float coarse_stop_point = powder->target_weight - powder->coarse_stop_threshold;

    // Coarse phase
    float coarse_over = overshoot(params->coarse_kp, params->coarse_kd,
                                  powder->coarse_kp_threshold, powder->coarse_overshoot_base,
                                  powder->coarse_overshoot_slope, powder->coarse_kd_damping);
    coarse_over *= 1.0f + powder->noise_amplitude * next_noise(powder);
    float coarse_time = duration(params->coarse_kp, params->coarse_kd,
                                 powder->coarse_time_base, powder->coarse_kd_slowdown);
    coarse_time *= 1.0f + powder->time_noise_amplitude * next_noise(powder);

    if (coarse_time > POWDER_ANALYSIS_TRIAL_TIMEOUT_S) {
        result->timed_out = true;
        result->weight = coarse_stop_point - 1.0f;      // never reached the stop point
        result->coarse_time_s = POWDER_ANALYSIS_TRIAL_TIMEOUT_S;
        result->total_time_s = POWDER_ANALYSIS_TRIAL_TIMEOUT_S;
        return;
    }

    result->weight = coarse_stop_point + coarse_over;
    result->coarse_time_s = coarse_time;
    result->total_time_s = coarse_time;

    if (params->throw_type == POWDER_ANALYSIS_THROW_COARSE_ONLY) {
        return;
    }

    // Fine phase: the fine trickler stops at target - fine_stop_threshold plus whatever still falls
    float fine_stop_point = powder->target_weight - powder->fine_stop_threshold;
    float fine_over = overshoot(params->fine_kp, params->fine_kd,
                                powder->fine_kp_threshold, powder->fine_overshoot_base,
                                powder->fine_overshoot_slope, powder->fine_kd_damping);
    fine_over *= 1.0f + powder->noise_amplitude * next_noise(powder);
    float fine_time = duration(params->fine_kp, params->fine_kd,
                               powder->fine_time_base, powder->fine_kd_slowdown);
    fine_time *= 1.0f + powder->time_noise_amplitude * next_noise(powder);

    if (coarse_time + fine_time > POWDER_ANALYSIS_TRIAL_TIMEOUT_S) {
        result->timed_out = true;
        result->weight = powder->target_weight - 1.0f;
        result->total_time_s = POWDER_ANALYSIS_TRIAL_TIMEOUT_S;
        return;
    }

    // A large coarse overshoot cannot be undone by the fine trickler; only when the fine stage
    // actually ran to its stop there is a stop weight
    float fine_weight = fine_stop_point + fine_over;
    if (fine_weight > result->weight) {
        result->weight = fine_weight;
        result->stop_weight = fine_stop_point;  // the sample that triggered the fine stop
    }
    result->total_time_s = coarse_time + fine_time;
}
