#include "ai_tuning.h"
#include "gp_lite.h"
#include "charge_mode.h"
#include "eeprom.h"
#include "profile.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Access charge mode config for thresholds
extern charge_mode_config_t charge_mode_config;

// EEPROM address defined in eeprom.h (EEPROM_AI_TUNING_HISTORY_BASE_ADDR = 12K)

// =============================================================================
// PARAMETER RANGES - Coarse is gentle (0-1), Fine is aggressive (0-10)
// =============================================================================
#define COARSE_KP_MIN 0.0f
#define COARSE_KP_MAX 1.0f
#define COARSE_KD_MIN 0.0f
#define COARSE_KD_MAX 1.0f

#define FINE_KP_MIN 0.0f
#define FINE_KP_MAX 10.0f
#define FINE_KD_MIN 0.0f
#define FINE_KD_MAX 10.0f

// Global tuning session state
static ai_tuning_session_t g_session;
static ai_tuning_config_t g_config;
static ai_tuning_history_t g_history;
static bool g_initialized = false;
static bool g_history_loaded = false;

// GP models for refinement phase
static gp_model_t g_gp_coarse;
static gp_model_t g_gp_fine;
static bool g_gp_initialized = false;

// Tuning state for adaptive Kp/Kd adjustment
typedef enum {
    TUNING_PHASE_ADAPTIVE_KP,    // Increasing Kp until overthrow
    TUNING_PHASE_ADAPTIVE_KD,    // Increasing Kd to reduce overthrow
    TUNING_PHASE_GP_REFINE       // GP-based refinement
} tuning_phase_t;

static tuning_phase_t g_coarse_tuning_phase;
static tuning_phase_t g_fine_tuning_phase;

// Adaptive step sizes - scaled for parameter ranges
static float g_coarse_kp_step;
static float g_coarse_kd_step;
static float g_fine_kp_step;
static float g_fine_kd_step;

// Track for oscillation detection
static bool g_coarse_had_overthrow;
static bool g_fine_had_overthrow;

// GP refinement tracking
static int g_gp_refine_drops;
#define GP_REFINE_DROPS 5  // Number of GP-guided drops for refinement

// Minimum step sizes based on range
#define COARSE_MIN_STEP 0.02f   // 2% of 0-1 range
#define FINE_MIN_STEP   0.2f    // 2% of 0-10 range

// Forward declarations
static void calculate_next_params_phase1(const ai_drop_telemetry_t* drop);
static void calculate_next_params_phase2(const ai_drop_telemetry_t* drop);
static void init_gp_models(void);
static float calculate_score(const ai_drop_telemetry_t* drop);
static void finalize_recommendations(void);

void ai_tuning_init(void) {
    if (g_initialized) {
        return;
    }

    // Initialize default configuration with correct ranges
    g_config.max_overthrow_percent = 6.67f;  // 1/15 overthrow
    g_config.target_coarse_time_ms = 10000.0f;
    g_config.target_total_time_ms = 15000.0f;

    // Parameter limits - COARSE: 0-1, FINE: 0-10
    g_config.coarse_kp_min = COARSE_KP_MIN;
    g_config.coarse_kp_max = COARSE_KP_MAX;
    g_config.coarse_kd_min = COARSE_KD_MIN;
    g_config.coarse_kd_max = COARSE_KD_MAX;

    g_config.fine_kp_min = FINE_KP_MIN;
    g_config.fine_kp_max = FINE_KP_MAX;
    g_config.fine_kd_min = FINE_KD_MIN;
    g_config.fine_kd_max = FINE_KD_MAX;

    // Default to quick mode
    g_config.tuning_mode = AI_TUNING_MODE_QUICK;

    // Clear session
    memset(&g_session, 0, sizeof(ai_tuning_session_t));
    g_session.state = AI_TUNING_IDLE;

    g_initialized = true;

    printf("AI Tuning System initialized\n");
    printf("  Coarse range: Kp 0-1, Kd 0-1\n");
    printf("  Fine range:   Kp 0-10, Kd 0-10\n");
}

static void init_gp_models(void) {
    if (g_gp_initialized) {
        gp_reset(&g_gp_coarse);
        gp_reset(&g_gp_fine);
    }

    // Initialize GP for coarse (0-1 range)
    gp_init(&g_gp_coarse,
            COARSE_KP_MIN, COARSE_KP_MAX,
            COARSE_KD_MIN, COARSE_KD_MAX);

    // Initialize GP for fine (0-10 range)
    gp_init(&g_gp_fine,
            FINE_KP_MIN, FINE_KP_MAX,
            FINE_KD_MIN, FINE_KD_MAX);

    g_gp_initialized = true;
    printf("GP models initialized for hybrid tuning\n");
}

// Calculate a score from drop telemetry (higher = better)
// Score considers: overthrow (bad), time (lower is better)
static float calculate_score(const ai_drop_telemetry_t* drop) {
    float score = 100.0f;

    // Penalize overthrow heavily (0-50 points penalty)
    float overthrow_penalty = fabsf(drop->overthrow_percent) * 5.0f;
    if (overthrow_penalty > 50.0f) overthrow_penalty = 50.0f;
    score -= overthrow_penalty;

    // Penalize slow time (0-30 points penalty)
    float time_ratio = drop->total_time_ms / g_config.target_total_time_ms;
    if (time_ratio > 1.0f) {
        float time_penalty = (time_ratio - 1.0f) * 30.0f;
        if (time_penalty > 30.0f) time_penalty = 30.0f;
        score -= time_penalty;
    }

    // Bonus for being under target time (0-20 points)
    if (time_ratio < 1.0f && drop->overthrow_percent <= g_config.max_overthrow_percent) {
        score += (1.0f - time_ratio) * 20.0f;
    }

    if (score < 0.0f) score = 0.0f;
    return score;
}

void ai_tuning_set_mode(ai_tuning_mode_t mode) {
    g_config.tuning_mode = mode;
    printf("AI Tuning: Mode set to %s\n",
           mode == AI_TUNING_MODE_QUICK ? "Quick" : "Fine");
}

ai_tuning_mode_t ai_tuning_get_mode(void) {
    return g_config.tuning_mode;
}

ai_tuning_config_t* ai_tuning_get_config(void) {
    return &g_config;
}

bool ai_tuning_start(profile_t* profile) {
    if (!g_initialized) {
        ai_tuning_init();
    }

    if (profile == NULL) {
        printf("AI Tuning: Invalid profile\n");
        return false;
    }

    // Use default time targets (10s coarse, 15s total)
    // These are set in ai_tuning_init() and can be overridden via config

    // Initialize GP models for hybrid tuning
    init_gp_models();

    // Initialize session
    memset(&g_session, 0, sizeof(ai_tuning_session_t));
    g_session.state = AI_TUNING_PHASE_1_COARSE;
    g_session.target_profile = profile;
    g_session.drops_completed = 0;
    g_session.total_drops_target = 15;    // Adaptive + GP refinement
    g_session.max_drops_allowed = 30;
    g_session.phase2_start_idx = 0;

    // Get profile index for ML suggestions
    uint8_t profile_idx = 0;
    for (int i = 0; i < 8; i++) {
        if (profile_select(i) == profile) {
            profile_idx = i;
            break;
        }
    }

    // Start from zero or use ML suggestions if available
    float suggested_coarse_kp, suggested_coarse_kd;
    float suggested_fine_kp, suggested_fine_kd;
    bool have_suggestions = ai_tuning_get_suggestions(profile_idx,
                                                       &suggested_coarse_kp, &suggested_coarse_kd,
                                                       &suggested_fine_kp, &suggested_fine_kd);

    if (have_suggestions) {
        // Start at 70% of suggested values
        g_session.coarse_kp_best = suggested_coarse_kp * 0.7f;
        g_session.coarse_kd_best = suggested_coarse_kd * 0.7f;
        g_session.fine_kp_best = suggested_fine_kp * 0.7f;
        g_session.fine_kd_best = suggested_fine_kd * 0.7f;
        printf("AI Tuning: Using ML suggestions as starting point (70%%)\n");
    } else {
        // Start at zero
        g_session.coarse_kp_best = 0.0f;
        g_session.coarse_kd_best = 0.0f;
        g_session.fine_kp_best = 0.0f;
        g_session.fine_kd_best = 0.0f;
    }

    g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;
    g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;

    // Initialize step sizes scaled for ranges
    // Coarse (0-1): start at 0.2 (20% of range)
    // Fine (0-10): start at 2.0 (20% of range)
    g_coarse_kp_step = 0.2f;
    g_coarse_kd_step = 0.1f;
    g_fine_kp_step = 2.0f;
    g_fine_kd_step = 1.0f;

    g_coarse_had_overthrow = false;
    g_fine_had_overthrow = false;
    g_gp_refine_drops = 0;

    printf("\n========================================================\n");
    printf("  HYBRID PID Auto-Tuning Started\n");
    printf("========================================================\n");
    printf("Profile: %s (idx %d)\n", profile->name, profile_idx);
    printf("Mode: %s\n", g_config.tuning_mode == AI_TUNING_MODE_QUICK ? "Quick" : "Fine");
    printf("\nPARAMETER RANGES:\n");
    printf("  Coarse: Kp 0.0-1.0, Kd 0.0-1.0\n");
    printf("  Fine:   Kp 0.0-10.0, Kd 0.0-10.0\n");
    printf("\nPHASE 1: Adaptive Coarse Tuning\n");
    printf("  - Kp steps: %.2f (halve on overshoot)\n", g_coarse_kp_step);
    printf("  - Goal: coarse time < %.0f ms\n", g_config.target_coarse_time_ms);
    printf("\nPHASE 2: Adaptive Fine Tuning\n");
    printf("  - Kp steps: %.1f (halve on overshoot)\n", g_fine_kp_step);
    printf("  - Goal: total time < %.0f ms, overthrow < %.1f%%\n",
           g_config.target_total_time_ms, g_config.max_overthrow_percent);
    printf("\nPHASE 3: GP Refinement (both motors)\n");
    printf("  - Uses Gaussian Process to find optimal Kp×Kd combo\n");
    printf("  - Leverages RP2350 FPU for fast matrix ops\n");
    printf("========================================================\n\n");

    return true;
}

bool ai_tuning_get_next_params(float* coarse_kp, float* coarse_kd,
                                 float* fine_kp, float* fine_kd) {
    if (!ai_tuning_is_active()) {
        return false;
    }

    if (g_session.state == AI_TUNING_PHASE_1_COARSE) {
        if (g_coarse_tuning_phase == TUNING_PHASE_GP_REFINE) {
            // GP suggests next coarse params
            gp_get_next_params(&g_gp_coarse, coarse_kp, coarse_kd);
        } else {
            *coarse_kp = g_session.coarse_kp_best;
            *coarse_kd = g_session.coarse_kd_best;
        }
        // Fine stays at minimum during coarse tuning
        *fine_kp = 0.0f;
        *fine_kd = 0.0f;
    }
    else if (g_session.state == AI_TUNING_PHASE_2_FINE) {
        // Use optimized coarse params
        *coarse_kp = g_session.recommended_coarse_kp;
        *coarse_kd = g_session.recommended_coarse_kd;

        if (g_fine_tuning_phase == TUNING_PHASE_GP_REFINE) {
            // GP suggests next fine params
            gp_get_next_params(&g_gp_fine, fine_kp, fine_kd);
        } else {
            *fine_kp = g_session.fine_kp_best;
            *fine_kd = g_session.fine_kd_best;
        }
    }
    else {
        return false;
    }

    return true;
}

bool ai_tuning_record_drop(const ai_drop_telemetry_t* telemetry) {
    if (!ai_tuning_is_active() || telemetry == NULL) {
        return false;
    }

    if (g_session.drops_completed >= g_session.max_drops_allowed) {
        printf("AI Tuning: Reached maximum %d drops without converging\n", g_session.max_drops_allowed);
        g_session.state = AI_TUNING_ERROR;
        snprintf(g_session.error_message, sizeof(g_session.error_message),
                 "Did not converge in %d drops", g_session.max_drops_allowed);
        g_session.recommended_coarse_kp = g_session.coarse_kp_best;
        g_session.recommended_coarse_kd = g_session.coarse_kd_best;
        g_session.recommended_fine_kp = g_session.fine_kp_best;
        g_session.recommended_fine_kd = g_session.fine_kd_best;
        return false;
    }

    // Store telemetry
    uint8_t idx = g_session.drops_completed;
    memcpy(&g_session.drops[idx], telemetry, sizeof(ai_drop_telemetry_t));
    g_session.drops_completed++;

    // Calculate score for GP
    float score = calculate_score(telemetry);

    printf("\n------------------------------------------------\n");
    printf("Drop %d completed (score: %.1f)\n", g_session.drops_completed, score);
    printf("------------------------------------------------\n");
    printf("Coarse: Kp=%.3f, Kd=%.3f (range 0-1)\n", telemetry->coarse_kp_used, telemetry->coarse_kd_used);
    printf("Fine:   Kp=%.2f, Kd=%.2f (range 0-10)\n", telemetry->fine_kp_used, telemetry->fine_kd_used);
    printf("Overthrow: %.3f gr (%.2f%%)\n", telemetry->overthrow, telemetry->overthrow_percent);
    printf("Time: %.0f ms (coarse: %.0f ms)\n", telemetry->total_time_ms, telemetry->coarse_time_ms);
    printf("------------------------------------------------\n");

    if (g_session.state == AI_TUNING_PHASE_1_COARSE) {
        // Add to GP model for coarse
        gp_add_observation(&g_gp_coarse,
                          telemetry->coarse_kp_used,
                          telemetry->coarse_kd_used,
                          score);
        calculate_next_params_phase1(telemetry);
    }
    else if (g_session.state == AI_TUNING_PHASE_2_FINE) {
        // Add to GP model for fine
        gp_add_observation(&g_gp_fine,
                          telemetry->fine_kp_used,
                          telemetry->fine_kd_used,
                          score);
        calculate_next_params_phase2(telemetry);
    }

    return true;
}

/**
 * Phase 1: Tune Coarse Trickler (Hybrid: Adaptive + GP)
 * Range: Kp 0-1, Kd 0-1
 *
 * Step 1: Adaptive - increase Kp until overthrow, then tune Kd
 * Step 2: GP Refine - use GP to find optimal Kp×Kd combination
 */
static void calculate_next_params_phase1(const ai_drop_telemetry_t* drop) {
    float coarse_threshold = charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;
    float target_time_ms = g_config.target_coarse_time_ms;

    bool has_overthrow = (drop->overthrow > coarse_threshold);
    bool time_ok = (drop->coarse_time_ms <= target_time_ms);

    printf("Phase 1 [%s]: overthrow=%.3f (thr=%.3f), time=%.0fms\n",
           g_coarse_tuning_phase == TUNING_PHASE_GP_REFINE ? "GP" : "Adaptive",
           drop->overthrow, coarse_threshold, drop->coarse_time_ms);

    if (g_coarse_tuning_phase == TUNING_PHASE_GP_REFINE) {
        // GP refinement phase
        g_gp_refine_drops++;

        if (g_gp_refine_drops >= GP_REFINE_DROPS) {
            // GP refinement complete - get best observed
            float best_score;
            gp_get_best_observed(&g_gp_coarse,
                                &g_session.recommended_coarse_kp,
                                &g_session.recommended_coarse_kd,
                                &best_score);

            printf("\n========================================================\n");
            printf("Phase 1 Complete - Coarse Trickler Tuned (Hybrid)\n");
            printf("========================================================\n");
            printf("Coarse: Kp=%.3f, Kd=%.3f (best score: %.1f)\n",
                   g_session.recommended_coarse_kp,
                   g_session.recommended_coarse_kd,
                   best_score);
            printf("========================================================\n\n");

            // Move to Phase 2
            g_session.phase2_start_idx = g_session.drops_completed;
            g_session.state = AI_TUNING_PHASE_2_FINE;
            g_gp_refine_drops = 0;

            printf("Starting Phase 2: Fine Trickler Tuning...\n\n");
        }
        return;
    }

    // Adaptive phase
    if (g_coarse_tuning_phase == TUNING_PHASE_ADAPTIVE_KP) {
        if (has_overthrow) {
            g_coarse_had_overthrow = true;

            if (g_coarse_kp_step > COARSE_MIN_STEP) {
                // Back off and halve step
                g_session.coarse_kp_best -= g_coarse_kp_step;
                if (g_session.coarse_kp_best < 0) g_session.coarse_kp_best = 0;
                g_coarse_kp_step /= 2.0f;
                if (g_coarse_kp_step < COARSE_MIN_STEP) g_coarse_kp_step = COARSE_MIN_STEP;

                printf("  Overthrow! Back off Kp to %.3f, step=%.3f\n",
                       g_session.coarse_kp_best, g_coarse_kp_step);

                g_session.coarse_kp_best += g_coarse_kp_step;
            } else {
                // Switch to Kd tuning
                printf("  Kp converged at %.3f, tuning Kd...\n", g_session.coarse_kp_best);
                g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
                g_session.coarse_kd_best += g_coarse_kd_step;
            }
        } else {
            // No overthrow - increase Kp
            g_session.coarse_kp_best += g_coarse_kp_step;
            printf("  No overthrow, Kp -> %.3f\n", g_session.coarse_kp_best);

            // Check if hit max
            if (g_session.coarse_kp_best >= COARSE_KP_MAX) {
                g_session.coarse_kp_best = COARSE_KP_MAX;
                printf("  Hit max Kp, switching to Kd tuning\n");
                g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            }
        }
    }
    else if (g_coarse_tuning_phase == TUNING_PHASE_ADAPTIVE_KD) {
        bool overthrow_ok = !has_overthrow;

        if (overthrow_ok && time_ok) {
            // Adaptive done - start GP refinement
            printf("\n  Adaptive coarse tuning found baseline.\n");
            printf("  Starting GP refinement to optimize Kp×Kd...\n\n");

            g_session.coarse_kp_best = fminf(g_session.coarse_kp_best, COARSE_KP_MAX);
            g_session.coarse_kd_best = fminf(g_session.coarse_kd_best, COARSE_KD_MAX);

            g_coarse_tuning_phase = TUNING_PHASE_GP_REFINE;
            g_gp_refine_drops = 0;
        }
        else if (!overthrow_ok) {
            // Still overthrowing - increase Kd
            g_session.coarse_kd_best += g_coarse_kd_step;
            printf("  Still overthrow, Kd -> %.3f\n", g_session.coarse_kd_best);

            if (g_session.coarse_kd_best >= COARSE_KD_MAX) {
                // Hit Kd limit - go to GP refinement anyway
                g_session.coarse_kd_best = COARSE_KD_MAX;
                printf("  Hit max Kd, starting GP refinement...\n");
                g_coarse_tuning_phase = TUNING_PHASE_GP_REFINE;
            }
        }
        else {
            // Time too slow - need more Kp
            g_session.coarse_kp_best += COARSE_MIN_STEP;
            printf("  Time slow, Kp -> %.3f\n", g_session.coarse_kp_best);
        }
    }

    // Clamp to valid range
    g_session.coarse_kp_best = fminf(fmaxf(g_session.coarse_kp_best, COARSE_KP_MIN), COARSE_KP_MAX);
    g_session.coarse_kd_best = fminf(fmaxf(g_session.coarse_kd_best, COARSE_KD_MIN), COARSE_KD_MAX);
}

/**
 * Phase 2: Tune Fine Trickler (Hybrid: Adaptive + GP)
 * Range: Kp 0-10, Kd 0-10
 *
 * Step 1: Adaptive - increase Kp until overthrow, then tune Kd
 * Step 2: GP Refine - use GP to find optimal Kp×Kd combination
 */
static void calculate_next_params_phase2(const ai_drop_telemetry_t* drop) {
    float max_overthrow_percent = g_config.max_overthrow_percent;
    float target_time_ms = g_config.target_total_time_ms;

    bool has_overthrow = (drop->overthrow > 0.0f);
    bool overthrow_acceptable = (fabsf(drop->overthrow_percent) <= max_overthrow_percent);
    bool time_ok = (drop->total_time_ms <= target_time_ms);

    printf("Phase 2 [%s]: overthrow=%.2f%% (max=%.1f%%), time=%.0fms\n",
           g_fine_tuning_phase == TUNING_PHASE_GP_REFINE ? "GP" : "Adaptive",
           drop->overthrow_percent, max_overthrow_percent, drop->total_time_ms);

    if (g_fine_tuning_phase == TUNING_PHASE_GP_REFINE) {
        // GP refinement phase
        g_gp_refine_drops++;

        if (g_gp_refine_drops >= GP_REFINE_DROPS) {
            // GP refinement complete
            float best_score;
            gp_get_best_observed(&g_gp_fine,
                                &g_session.recommended_fine_kp,
                                &g_session.recommended_fine_kd,
                                &best_score);

            finalize_recommendations();
        }
        return;
    }

    // Adaptive phase
    if (g_fine_tuning_phase == TUNING_PHASE_ADAPTIVE_KP) {
        if (has_overthrow) {
            g_fine_had_overthrow = true;

            if (g_fine_kp_step > FINE_MIN_STEP) {
                // Back off and halve step
                g_session.fine_kp_best -= g_fine_kp_step;
                if (g_session.fine_kp_best < 0) g_session.fine_kp_best = 0;
                g_fine_kp_step /= 2.0f;
                if (g_fine_kp_step < FINE_MIN_STEP) g_fine_kp_step = FINE_MIN_STEP;

                printf("  Overthrow! Back off Kp to %.2f, step=%.2f\n",
                       g_session.fine_kp_best, g_fine_kp_step);

                g_session.fine_kp_best += g_fine_kp_step;
            } else {
                // Switch to Kd tuning
                printf("  Kp converged at %.2f, tuning Kd...\n", g_session.fine_kp_best);
                g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
                g_session.fine_kd_best += g_fine_kd_step;
            }
        } else {
            // No overthrow - increase Kp
            g_session.fine_kp_best += g_fine_kp_step;
            printf("  No overthrow, Kp -> %.2f\n", g_session.fine_kp_best);

            if (g_session.fine_kp_best >= FINE_KP_MAX) {
                g_session.fine_kp_best = FINE_KP_MAX;
                printf("  Hit max Kp, switching to Kd tuning\n");
                g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            }
        }
    }
    else if (g_fine_tuning_phase == TUNING_PHASE_ADAPTIVE_KD) {
        if (overthrow_acceptable && time_ok) {
            // Adaptive done - start GP refinement
            printf("\n  Adaptive fine tuning found baseline.\n");
            printf("  Starting GP refinement to optimize Kp×Kd...\n\n");

            g_session.fine_kp_best = fminf(g_session.fine_kp_best, FINE_KP_MAX);
            g_session.fine_kd_best = fminf(g_session.fine_kd_best, FINE_KD_MAX);

            g_fine_tuning_phase = TUNING_PHASE_GP_REFINE;
            g_gp_refine_drops = 0;
        }
        else if (!overthrow_acceptable && has_overthrow) {
            // Still overthrowing too much - increase Kd
            g_session.fine_kd_best += g_fine_kd_step;
            printf("  Overthrow high (%.2f%%), Kd -> %.2f\n",
                   drop->overthrow_percent, g_session.fine_kd_best);

            if (g_session.fine_kd_best >= FINE_KD_MAX) {
                g_session.fine_kd_best = FINE_KD_MAX;
                printf("  Hit max Kd, starting GP refinement...\n");
                g_fine_tuning_phase = TUNING_PHASE_GP_REFINE;
            }
        }
        else if (!time_ok) {
            // Time too slow
            g_session.fine_kp_best += FINE_MIN_STEP;
            printf("  Time slow (%.0fms), Kp -> %.2f\n",
                   drop->total_time_ms, g_session.fine_kp_best);
        }
        else {
            // Underthrow - reduce Kd slightly
            if (g_session.fine_kd_best > FINE_MIN_STEP) {
                g_session.fine_kd_best -= FINE_MIN_STEP;
                printf("  Underthrow, Kd -> %.2f\n", g_session.fine_kd_best);
            }
        }
    }

    // Clamp to valid range
    g_session.fine_kp_best = fminf(fmaxf(g_session.fine_kp_best, FINE_KP_MIN), FINE_KP_MAX);
    g_session.fine_kd_best = fminf(fmaxf(g_session.fine_kd_best, FINE_KD_MIN), FINE_KD_MAX);
}

static void finalize_recommendations(void) {
    g_session.state = AI_TUNING_COMPLETE;

    // Calculate average stats
    float total_overthrow = 0.0f;
    float total_time = 0.0f;
    int count = 0;

    for (int i = 0; i < g_session.drops_completed; i++) {
        total_overthrow += g_session.drops[i].overthrow;
        total_time += g_session.drops[i].total_time_ms;
        count++;
    }

    if (count > 0) {
        g_session.avg_overthrow = total_overthrow / count;
        g_session.avg_total_time = total_time / count;
    }

    printf("\n========================================================\n");
    printf("  HYBRID AI PID Auto-Tuning COMPLETE!\n");
    printf("========================================================\n");
    printf("\nRECOMMENDED PARAMETERS:\n");
    printf("  Coarse (0-1 range):\n");
    printf("    Kp: %.3f  Kd: %.3f  Ki: 0\n",
           g_session.recommended_coarse_kp, g_session.recommended_coarse_kd);
    printf("  Fine (0-10 range):\n");
    printf("    Kp: %.2f  Kd: %.2f  Ki: 0\n",
           g_session.recommended_fine_kp, g_session.recommended_fine_kd);
    printf("\nSTATISTICS:\n");
    printf("  Total drops: %d\n", g_session.drops_completed);
    printf("  Avg overthrow: %.3f grains\n", g_session.avg_overthrow);
    printf("  Avg time: %.0f ms\n", g_session.avg_total_time);
    printf("\nPlease review and confirm to apply these parameters.\n");
    printf("========================================================\n\n");
}

bool ai_tuning_is_complete(void) {
    return g_session.state == AI_TUNING_COMPLETE;
}

ai_tuning_session_t* ai_tuning_get_session(void) {
    return &g_session;
}

bool ai_tuning_get_recommended_params(float* coarse_kp, float* coarse_kd,
                                       float* fine_kp, float* fine_kd) {
    if (g_session.state != AI_TUNING_COMPLETE && g_session.state != AI_TUNING_ERROR) {
        return false;
    }

    *coarse_kp = g_session.recommended_coarse_kp;
    *coarse_kd = g_session.recommended_coarse_kd;
    *fine_kp = g_session.recommended_fine_kp;
    *fine_kd = g_session.recommended_fine_kd;

    return true;
}

bool ai_tuning_apply_params(void) {
    if ((g_session.state != AI_TUNING_COMPLETE && g_session.state != AI_TUNING_ERROR) ||
        g_session.target_profile == NULL) {
        return false;
    }

    // Apply recommended parameters to profile
    g_session.target_profile->coarse_kp = g_session.recommended_coarse_kp;
    g_session.target_profile->coarse_kd = g_session.recommended_coarse_kd;
    g_session.target_profile->fine_kp = g_session.recommended_fine_kp;
    g_session.target_profile->fine_kd = g_session.recommended_fine_kd;

    printf("AI Tuning: Parameters applied to profile '%s'\n", g_session.target_profile->name);
    printf("  Coarse: Kp=%.3f Kd=%.3f (range 0-1)\n",
           g_session.target_profile->coarse_kp, g_session.target_profile->coarse_kd);
    printf("  Fine:   Kp=%.2f Kd=%.2f (range 0-10)\n",
           g_session.target_profile->fine_kp, g_session.target_profile->fine_kd);

    g_session.state = AI_TUNING_IDLE;

    return true;
}

void ai_tuning_cancel(void) {
    printf("AI Tuning: Session cancelled\n");
    g_session.state = AI_TUNING_IDLE;
    memset(&g_session, 0, sizeof(ai_tuning_session_t));
}

bool ai_tuning_is_active(void) {
    return g_session.state == AI_TUNING_PHASE_1_COARSE ||
           g_session.state == AI_TUNING_PHASE_2_FINE;
}

ai_motor_mode_t ai_tuning_get_motor_mode(void) {
    switch (g_session.state) {
        case AI_TUNING_PHASE_1_COARSE:
            return AI_MOTOR_MODE_COARSE_ONLY;
        case AI_TUNING_PHASE_2_FINE:
            return AI_MOTOR_MODE_FINE_ONLY;
        default:
            return AI_MOTOR_MODE_NORMAL;
    }
}

uint8_t ai_tuning_get_progress_percent(void) {
    if (g_session.total_drops_target == 0) {
        return 0;
    }
    return (100 * g_session.drops_completed) / g_session.total_drops_target;
}

// ============================================================================
// ML Learning Functions - Records every charge and refines PID values
// ============================================================================

static void load_history_from_eeprom(void) {
    if (g_history_loaded) {
        return;
    }

    bool ok = eeprom_read(EEPROM_AI_TUNING_HISTORY_BASE_ADDR, (uint8_t*)&g_history, sizeof(ai_tuning_history_t));

    if (!ok || g_history.revision != AI_TUNING_HISTORY_REV) {
        printf("AI ML: Initializing fresh history\n");
        memset(&g_history, 0, sizeof(ai_tuning_history_t));
        g_history.revision = AI_TUNING_HISTORY_REV;
    } else {
        printf("AI ML: Loaded %d drop records\n", g_history.count);
    }

    g_history_loaded = true;
}

static void save_history_to_eeprom(void) {
    eeprom_write(EEPROM_AI_TUNING_HISTORY_BASE_ADDR, (uint8_t*)&g_history, sizeof(ai_tuning_history_t));
}

ai_tuning_history_t* ai_tuning_get_history(void) {
    load_history_from_eeprom();
    return &g_history;
}

void ai_tuning_record_charge(uint8_t profile_idx,
                              float coarse_kp, float coarse_kd,
                              float fine_kp, float fine_kd,
                              float overthrow,
                              float coarse_time_ms, float fine_time_ms) {
    load_history_from_eeprom();

    // Create drop record
    ai_drop_record_t record = {
        .coarse_kp = coarse_kp,
        .coarse_kd = coarse_kd,
        .fine_kp = fine_kp,
        .fine_kd = fine_kd,
        .overthrow = overthrow,
        .coarse_time_ms = coarse_time_ms,
        .fine_time_ms = fine_time_ms,
        .total_time_ms = coarse_time_ms + fine_time_ms,
        .profile_idx = profile_idx
    };

    // Add to circular buffer
    g_history.drops[g_history.next_idx] = record;
    g_history.next_idx = (g_history.next_idx + 1) % AI_TUNING_HISTORY_SIZE;
    if (g_history.count < AI_TUNING_HISTORY_SIZE) {
        g_history.count++;
    }

    printf("AI ML: Recorded drop #%d (overthrow=%.3f, time=%.0fms)\n",
           g_history.count, overthrow, coarse_time_ms + fine_time_ms);

    // Auto-calculate refinements when we have enough data
    if (g_history.count >= 3) {
        ai_tuning_calculate_refinements();
    }

    save_history_to_eeprom();
}

void ai_tuning_calculate_refinements(void) {
    load_history_from_eeprom();

    if (g_history.count < 3) {
        g_history.has_suggestions = false;
        return;
    }

    // Analyze drops and calculate suggested adjustments
    float avg_overthrow = 0.0f;
    float avg_coarse_kp = 0.0f, avg_coarse_kd = 0.0f;
    float avg_fine_kp = 0.0f, avg_fine_kd = 0.0f;

    for (int i = 0; i < g_history.count; i++) {
        ai_drop_record_t* d = &g_history.drops[i];
        avg_overthrow += d->overthrow;
        avg_coarse_kp += d->coarse_kp;
        avg_coarse_kd += d->coarse_kd;
        avg_fine_kp += d->fine_kp;
        avg_fine_kd += d->fine_kd;
    }

    avg_overthrow /= g_history.count;
    avg_coarse_kp /= g_history.count;
    avg_coarse_kd /= g_history.count;
    avg_fine_kp /= g_history.count;
    avg_fine_kd /= g_history.count;

    // Calculate adjustments based on overthrow pattern
    float coarse_threshold = charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;
    float fine_threshold = charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;

    float coarse_kp_adj = 0.0f, coarse_kd_adj = 0.0f;
    float fine_kp_adj = 0.0f, fine_kd_adj = 0.0f;

    // Coarse adjustments (scaled for 0-1 range)
    if (avg_overthrow > coarse_threshold * 0.5f) {
        coarse_kd_adj = 0.01f;  // Small adjustment for 0-1 range
    } else if (avg_overthrow < -fine_threshold) {
        coarse_kp_adj = 0.01f;
    }

    // Fine adjustments (scaled for 0-10 range)
    if (avg_overthrow > fine_threshold) {
        fine_kd_adj = 0.1f;  // Larger adjustment for 0-10 range
    } else if (avg_overthrow < -fine_threshold) {
        fine_kp_adj = 0.1f;
    }

    // Apply adjustments with range clamping
    g_history.suggested_coarse_kp = fminf(fmaxf(avg_coarse_kp + coarse_kp_adj, COARSE_KP_MIN), COARSE_KP_MAX);
    g_history.suggested_coarse_kd = fminf(fmaxf(avg_coarse_kd + coarse_kd_adj, COARSE_KD_MIN), COARSE_KD_MAX);
    g_history.suggested_fine_kp = fminf(fmaxf(avg_fine_kp + fine_kp_adj, FINE_KP_MIN), FINE_KP_MAX);
    g_history.suggested_fine_kd = fminf(fmaxf(avg_fine_kd + fine_kd_adj, FINE_KD_MIN), FINE_KD_MAX);
    g_history.has_suggestions = true;

    printf("AI ML: Refined values (avg overthrow=%.3f):\n", avg_overthrow);
    printf("  Coarse (0-1): Kp %.3f->%.3f, Kd %.3f->%.3f\n",
           avg_coarse_kp, g_history.suggested_coarse_kp,
           avg_coarse_kd, g_history.suggested_coarse_kd);
    printf("  Fine (0-10):  Kp %.2f->%.2f, Kd %.2f->%.2f\n",
           avg_fine_kp, g_history.suggested_fine_kp,
           avg_fine_kd, g_history.suggested_fine_kd);
}

bool ai_tuning_get_refined_params(float* coarse_kp, float* coarse_kd,
                                   float* fine_kp, float* fine_kd) {
    load_history_from_eeprom();

    if (!g_history.has_suggestions) {
        return false;
    }

    *coarse_kp = g_history.suggested_coarse_kp;
    *coarse_kd = g_history.suggested_coarse_kd;
    *fine_kp = g_history.suggested_fine_kp;
    *fine_kd = g_history.suggested_fine_kd;

    return true;
}

bool ai_tuning_get_suggestions(uint8_t profile_idx,
                                float* coarse_kp, float* coarse_kd,
                                float* fine_kp, float* fine_kd) {
    load_history_from_eeprom();

    // Need at least 3 drops for suggestions
    if (g_history.count < 3) {
        return false;
    }

    // Calculate average PID values from history for this profile
    float sum_coarse_kp = 0, sum_coarse_kd = 0;
    float sum_fine_kp = 0, sum_fine_kd = 0;
    int count = 0;

    for (int i = 0; i < g_history.count; i++) {
        ai_drop_record_t* d = &g_history.drops[i];
        // Filter by profile if specified, or use all if profile_idx == 0xFF
        if (profile_idx == 0xFF || d->profile_idx == profile_idx) {
            sum_coarse_kp += d->coarse_kp;
            sum_coarse_kd += d->coarse_kd;
            sum_fine_kp += d->fine_kp;
            sum_fine_kd += d->fine_kd;
            count++;
        }
    }

    if (count < 3) {
        // Not enough data for this profile, try global suggestions
        if (g_history.has_suggestions) {
            *coarse_kp = g_history.suggested_coarse_kp;
            *coarse_kd = g_history.suggested_coarse_kd;
            *fine_kp = g_history.suggested_fine_kp;
            *fine_kd = g_history.suggested_fine_kd;
            return true;
        }
        return false;
    }

    *coarse_kp = sum_coarse_kp / count;
    *coarse_kd = sum_coarse_kd / count;
    *fine_kp = sum_fine_kp / count;
    *fine_kd = sum_fine_kd / count;

    printf("AI ML: Suggestions for profile %d (from %d drops):\n", profile_idx, count);
    printf("  Coarse: Kp=%.3f, Kd=%.3f\n", *coarse_kp, *coarse_kd);
    printf("  Fine:   Kp=%.2f, Kd=%.2f\n", *fine_kp, *fine_kd);

    return true;
}

bool ai_tuning_apply_refined_params(uint8_t profile_idx) {
    load_history_from_eeprom();

    if (!g_history.has_suggestions) {
        printf("AI ML: No suggestions to apply\n");
        return false;
    }

    profile_t* profile = profile_select(profile_idx);
    if (!profile) {
        printf("AI ML: Invalid profile %d\n", profile_idx);
        return false;
    }

    profile->coarse_kp = g_history.suggested_coarse_kp;
    profile->coarse_kd = g_history.suggested_coarse_kd;
    profile->fine_kp = g_history.suggested_fine_kp;
    profile->fine_kd = g_history.suggested_fine_kd;

    profile_data_save();

    printf("AI ML: Applied refined values to profile '%s'\n", profile->name);
    printf("  Coarse (0-1): Kp=%.3f, Kd=%.3f\n", profile->coarse_kp, profile->coarse_kd);
    printf("  Fine (0-10):  Kp=%.2f, Kd=%.2f\n", profile->fine_kp, profile->fine_kd);

    // Clear history after applying (start fresh learning)
    g_history.count = 0;
    g_history.next_idx = 0;
    g_history.has_suggestions = false;
    save_history_to_eeprom();

    return true;
}

void ai_tuning_clear_history(void) {
    memset(&g_history, 0, sizeof(ai_tuning_history_t));
    g_history.revision = AI_TUNING_HISTORY_REV;
    save_history_to_eeprom();
    printf("AI ML: History cleared\n");
}
