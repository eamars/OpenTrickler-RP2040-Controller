#ifndef GP_LITE_H_
#define GP_LITE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * Lightweight Gaussian Process for RP2350
 *
 * Optimized for:
 * - Small datasets (max 20 points)
 * - 2D parameter space (Kp, Kd)
 * - Squared exponential kernel
 * - UCB acquisition function
 *
 * Memory: ~3KB for 20 points
 * Compute: ~10-50ms per prediction
 */

#define GP_MAX_POINTS 20
#define GP_PARAM_DIM 2      // Kp, Kd

// Observation point
typedef struct {
    float params[GP_PARAM_DIM];  // [Kp, Kd]
    float score;                  // Observed score (higher = better)
} gp_observation_t;

// GP model state
typedef struct {
    // Observations
    gp_observation_t obs[GP_MAX_POINTS];
    uint8_t n_obs;

    // Kernel hyperparameters
    float length_scale;    // Controls smoothness
    float signal_var;      // Signal variance
    float noise_var;       // Observation noise

    // Precomputed matrices (updated after each observation)
    float K_inv[GP_MAX_POINTS][GP_MAX_POINTS];  // Inverse of K + noise*I
    float alpha[GP_MAX_POINTS];                  // K_inv * y (for fast prediction)
    bool matrices_valid;

    // Parameter bounds
    float param_min[GP_PARAM_DIM];
    float param_max[GP_PARAM_DIM];

    // UCB exploration parameter
    float beta;  // Higher = more exploration
} gp_model_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize GP model
 */
void gp_init(gp_model_t* gp,
             float kp_min, float kp_max,
             float kd_min, float kd_max);

/**
 * Add observation to GP
 * Returns false if max observations reached
 */
bool gp_add_observation(gp_model_t* gp, float kp, float kd, float score);

/**
 * Predict mean and variance at a point
 */
void gp_predict(gp_model_t* gp, float kp, float kd,
                float* mean, float* variance);

/**
 * Compute UCB acquisition value at a point
 * UCB = mean + beta * sqrt(variance)
 */
float gp_ucb(gp_model_t* gp, float kp, float kd);

/**
 * Find next parameters to try using UCB
 * Searches grid and returns best acquisition point
 */
void gp_get_next_params(gp_model_t* gp, float* kp, float* kd);

/**
 * Get best observed parameters so far
 */
void gp_get_best_observed(gp_model_t* gp, float* kp, float* kd, float* score);

/**
 * Reset GP model (clear observations)
 */
void gp_reset(gp_model_t* gp);

#ifdef __cplusplus
}
#endif

#endif // GP_LITE_H_
