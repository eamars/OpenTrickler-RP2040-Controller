#include "gp_lite.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
static float kernel(gp_model_t* gp, float* x1, float* x2);
static bool cholesky_decompose(float L[GP_MAX_POINTS][GP_MAX_POINTS], int n);
static void cholesky_solve(float L[GP_MAX_POINTS][GP_MAX_POINTS], float* b, float* x, int n);
static void update_matrices(gp_model_t* gp);

void gp_init(gp_model_t* gp,
             float kp_min, float kp_max,
             float kd_min, float kd_max) {
    memset(gp, 0, sizeof(gp_model_t));

    // Parameter bounds
    gp->param_min[0] = kp_min;
    gp->param_max[0] = kp_max;
    gp->param_min[1] = kd_min;
    gp->param_max[1] = kd_max;

    // Kernel hyperparameters - tuned for PID parameter space
    // Length scale: ~10% of parameter range for smooth interpolation
    float kp_range = kp_max - kp_min;
    float kd_range = kd_max - kd_min;
    gp->length_scale = fmaxf(kp_range, kd_range) * 0.15f;

    gp->signal_var = 100.0f;   // Score variance (scores are 0-100)
    gp->noise_var = 5.0f;      // Observation noise

    // UCB exploration parameter
    // Higher = more exploration, lower = more exploitation
    gp->beta = 2.0f;

    gp->n_obs = 0;
    gp->matrices_valid = false;

    printf("GP Lite initialized: length_scale=%.2f, beta=%.2f\n",
           gp->length_scale, gp->beta);
}

// Squared exponential (RBF) kernel
static float kernel(gp_model_t* gp, float* x1, float* x2) {
    float sq_dist = 0.0f;
    for (int i = 0; i < GP_PARAM_DIM; i++) {
        float diff = x1[i] - x2[i];
        sq_dist += diff * diff;
    }

    // k(x1, x2) = signal_var * exp(-0.5 * ||x1-x2||^2 / length_scale^2)
    float ls_sq = gp->length_scale * gp->length_scale;
    return gp->signal_var * expf(-0.5f * sq_dist / ls_sq);
}

// Cholesky decomposition: A = L * L^T
// Returns false if matrix is not positive definite
static bool cholesky_decompose(float L[GP_MAX_POINTS][GP_MAX_POINTS], int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            float sum = L[i][j];

            for (int k = 0; k < j; k++) {
                sum -= L[i][k] * L[j][k];
            }

            if (i == j) {
                if (sum <= 0.0f) {
                    // Not positive definite - add jitter
                    sum = 1e-6f;
                }
                L[i][j] = sqrtf(sum);
            } else {
                L[i][j] = sum / L[j][j];
            }
        }
        // Zero out upper triangle
        for (int j = i + 1; j < n; j++) {
            L[i][j] = 0.0f;
        }
    }
    return true;
}

// Solve L * L^T * x = b using forward/backward substitution
static void cholesky_solve(float L[GP_MAX_POINTS][GP_MAX_POINTS], float* b, float* x, int n) {
    float y[GP_MAX_POINTS];

    // Forward substitution: L * y = b
    for (int i = 0; i < n; i++) {
        float sum = b[i];
        for (int j = 0; j < i; j++) {
            sum -= L[i][j] * y[j];
        }
        y[i] = sum / L[i][i];
    }

    // Backward substitution: L^T * x = y
    for (int i = n - 1; i >= 0; i--) {
        float sum = y[i];
        for (int j = i + 1; j < n; j++) {
            sum -= L[j][i] * x[j];
        }
        x[i] = sum / L[i][i];
    }
}

// Update K_inv and alpha after new observation
static void update_matrices(gp_model_t* gp) {
    int n = gp->n_obs;
    if (n == 0) {
        gp->matrices_valid = false;
        return;
    }

    // Build covariance matrix K + noise*I
    float K[GP_MAX_POINTS][GP_MAX_POINTS];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            K[i][j] = kernel(gp, gp->obs[i].params, gp->obs[j].params);
            if (i == j) {
                K[i][j] += gp->noise_var;  // Add noise on diagonal
            }
        }
    }

    // Cholesky decomposition
    float L[GP_MAX_POINTS][GP_MAX_POINTS];
    memcpy(L, K, sizeof(L));
    if (!cholesky_decompose(L, n)) {
        printf("GP: Cholesky failed, adding jitter\n");
        // Add more jitter and retry
        for (int i = 0; i < n; i++) {
            K[i][i] += 1e-4f;
        }
        memcpy(L, K, sizeof(L));
        cholesky_decompose(L, n);
    }

    // Compute alpha = K_inv * y for fast prediction
    float y[GP_MAX_POINTS];
    for (int i = 0; i < n; i++) {
        y[i] = gp->obs[i].score;
    }
    cholesky_solve(L, y, gp->alpha, n);

    // Store L in K_inv for later use (we use L, not actual K_inv)
    memcpy(gp->K_inv, L, sizeof(gp->K_inv));

    gp->matrices_valid = true;
}

bool gp_add_observation(gp_model_t* gp, float kp, float kd, float score) {
    if (gp->n_obs >= GP_MAX_POINTS) {
        printf("GP: Max observations reached (%d)\n", GP_MAX_POINTS);
        return false;
    }

    gp_observation_t* obs = &gp->obs[gp->n_obs];
    obs->params[0] = kp;
    obs->params[1] = kd;
    obs->score = score;
    gp->n_obs++;

    // Invalidate and recompute matrices
    gp->matrices_valid = false;
    update_matrices(gp);

    printf("GP: Added obs %d: Kp=%.3f Kd=%.3f score=%.1f\n",
           gp->n_obs, kp, kd, score);

    return true;
}

void gp_predict(gp_model_t* gp, float kp, float kd,
                float* mean, float* variance) {
    int n = gp->n_obs;

    // No observations - return prior
    if (n == 0 || !gp->matrices_valid) {
        *mean = 50.0f;  // Prior mean (middle of score range)
        *variance = gp->signal_var;
        return;
    }

    float x[GP_PARAM_DIM] = {kp, kd};

    // Compute k_star = [k(x, x1), k(x, x2), ...]
    float k_star[GP_MAX_POINTS];
    for (int i = 0; i < n; i++) {
        k_star[i] = kernel(gp, x, gp->obs[i].params);
    }

    // Mean: mu = k_star^T * alpha
    float mu = 0.0f;
    for (int i = 0; i < n; i++) {
        mu += k_star[i] * gp->alpha[i];
    }
    *mean = mu;

    // Variance: var = k(x,x) - k_star^T * K_inv * k_star
    // We have L from Cholesky, so solve L * v = k_star
    float v[GP_MAX_POINTS];
    for (int i = 0; i < n; i++) {
        float sum = k_star[i];
        for (int j = 0; j < i; j++) {
            sum -= gp->K_inv[i][j] * v[j];
        }
        v[i] = sum / gp->K_inv[i][i];
    }

    // var = k(x,x) - v^T * v
    float k_xx = kernel(gp, x, x);
    float v_dot_v = 0.0f;
    for (int i = 0; i < n; i++) {
        v_dot_v += v[i] * v[i];
    }
    *variance = fmaxf(0.0f, k_xx - v_dot_v);
}

float gp_ucb(gp_model_t* gp, float kp, float kd) {
    float mean, variance;
    gp_predict(gp, kp, kd, &mean, &variance);

    // UCB = mean + beta * sqrt(variance)
    return mean + gp->beta * sqrtf(variance);
}

void gp_get_next_params(gp_model_t* gp, float* kp, float* kd) {
    // Grid search for max UCB
    // Use 10x10 grid for speed (100 evaluations)
    int grid_size = 10;

    float best_ucb = -1e9f;
    float best_kp = (gp->param_min[0] + gp->param_max[0]) / 2.0f;
    float best_kd = (gp->param_min[1] + gp->param_max[1]) / 2.0f;

    float kp_step = (gp->param_max[0] - gp->param_min[0]) / (float)(grid_size - 1);
    float kd_step = (gp->param_max[1] - gp->param_min[1]) / (float)(grid_size - 1);

    for (int i = 0; i < grid_size; i++) {
        for (int j = 0; j < grid_size; j++) {
            float test_kp = gp->param_min[0] + i * kp_step;
            float test_kd = gp->param_min[1] + j * kd_step;

            float ucb = gp_ucb(gp, test_kp, test_kd);

            if (ucb > best_ucb) {
                best_ucb = ucb;
                best_kp = test_kp;
                best_kd = test_kd;
            }
        }
    }

    // Local refinement around best point (2x2 grid, finer resolution)
    float refine_range = fmaxf(kp_step, kd_step) * 0.5f;
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            float test_kp = best_kp + i * refine_range * 0.5f;
            float test_kd = best_kd + j * refine_range * 0.5f;

            // Clamp to bounds
            test_kp = fmaxf(gp->param_min[0], fminf(test_kp, gp->param_max[0]));
            test_kd = fmaxf(gp->param_min[1], fminf(test_kd, gp->param_max[1]));

            float ucb = gp_ucb(gp, test_kp, test_kd);

            if (ucb > best_ucb) {
                best_ucb = ucb;
                best_kp = test_kp;
                best_kd = test_kd;
            }
        }
    }

    *kp = best_kp;
    *kd = best_kd;

    printf("GP: Next params Kp=%.3f Kd=%.3f (UCB=%.1f)\n", best_kp, best_kd, best_ucb);
}

void gp_get_best_observed(gp_model_t* gp, float* kp, float* kd, float* score) {
    if (gp->n_obs == 0) {
        *kp = (gp->param_min[0] + gp->param_max[0]) / 2.0f;
        *kd = (gp->param_min[1] + gp->param_max[1]) / 2.0f;
        *score = 0.0f;
        return;
    }

    int best_idx = 0;
    float best_score = gp->obs[0].score;

    for (int i = 1; i < gp->n_obs; i++) {
        if (gp->obs[i].score > best_score) {
            best_score = gp->obs[i].score;
            best_idx = i;
        }
    }

    *kp = gp->obs[best_idx].params[0];
    *kd = gp->obs[best_idx].params[1];
    *score = best_score;
}

void gp_reset(gp_model_t* gp) {
    gp->n_obs = 0;
    gp->matrices_valid = false;
    memset(gp->obs, 0, sizeof(gp->obs));
    memset(gp->K_inv, 0, sizeof(gp->K_inv));
    memset(gp->alpha, 0, sizeof(gp->alpha));
}
