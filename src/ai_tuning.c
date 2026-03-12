/**
 * @file ai_tuning.c
 * @brief AI Auto-Tuning Implementation for RP2040
 *
 * Binary step algorithm for tuning Kp/Kd parameters.
 */

#include "ai_tuning.h"
#include "charge_mode.h"
#include "flash_storage.h"
#include "eeprom.h"
#include "profile.h"
#include <string.h>
#include <math.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "encoder.h"

// Access charge mode config for thresholds
extern charge_mode_config_t charge_mode_config;

// Thread safety mutex
static SemaphoreHandle_t g_ai_tuning_mutex = NULL;

// =============================================================================
// PARAMETER RANGES - Coarse is gentle (0-1), Fine is aggressive (0-10)
// =============================================================================
#define COARSE_KP_MIN 0.01f
#define COARSE_KP_MAX 1.0f
#define COARSE_KD_MIN 0.01f
#define COARSE_KD_MAX 2.0f

#define FINE_KP_MIN 0.01f
#define FINE_KP_MAX 5.0f
#define FINE_KD_MIN 0.01f
#define FINE_KD_MAX 20.0f

// Minimum step sizes based on range
#define COARSE_MIN_STEP 0.02f
#define FINE_MIN_STEP   0.1f

// Global tuning session state
static ai_tuning_session_t g_session;
static ai_tuning_config_t g_config;
static ai_tuning_config_eeprom_t g_config_eeprom;
static ai_tuning_history_t g_history;
static bool g_initialized = false;
static bool g_history_loaded = false;
static bool g_config_loaded = false;

// Original profile values saved at tuning start (for cancel/restore)
static float g_orig_coarse_kp = 0.0f;
static float g_orig_coarse_kd = 0.0f;
static float g_orig_fine_kp = 0.0f;
static float g_orig_fine_kd = 0.0f;

// Tuning state for adaptive Kp/Kd adjustment
typedef enum {
    TUNING_PHASE_ADAPTIVE_KP,
    TUNING_PHASE_ADAPTIVE_KD
} tuning_phase_t;

static tuning_phase_t g_coarse_tuning_phase;
static tuning_phase_t g_fine_tuning_phase;

// Adaptive step sizes
static float g_coarse_kp_step;
static float g_coarse_kd_step;
static float g_fine_kp_step;
static float g_fine_kd_step;

// Track for oscillation detection
static bool g_coarse_had_overthrow;
static bool g_fine_had_overthrow;

// Forward declarations
static void calculate_next_params_phase1(const ai_drop_telemetry_t* drop);
static void calculate_next_params_phase2(const ai_drop_telemetry_t* drop);
static void load_history_from_flash(void);
static void save_history_to_flash(void);
static void load_config_from_eeprom(void);
static bool save_config_to_eeprom(void);

// =============================================================================
// Initialization
// =============================================================================

void ai_tuning_init(void) {
    if (g_initialized) {
        return;
    }

    // Create mutex for thread safety
    if (g_ai_tuning_mutex == NULL) {
        g_ai_tuning_mutex = xSemaphoreCreateRecursiveMutex();
    }

    // Initialize default configuration
    g_config.max_overthrow_percent = 6.67f;

    g_config.coarse_kp_min = COARSE_KP_MIN;
    g_config.coarse_kp_max = COARSE_KP_MAX;
    g_config.coarse_kd_min = COARSE_KD_MIN;
    g_config.coarse_kd_max = COARSE_KD_MAX;

    g_config.fine_kp_min = FINE_KP_MIN;
    g_config.fine_kp_max = FINE_KP_MAX;
    g_config.fine_kd_min = FINE_KD_MIN;
    g_config.fine_kd_max = FINE_KD_MAX;

    g_config.noise_margin = 0.05f;              // ±0.05gn for fine Kd phase

    // Tuning acceptance factors
    g_config.coarse_kp_max_factor = 1.02f;      // Kp phase: threshold <= drop <= threshold * 1.02
    g_config.coarse_kd_max_factor = 1.01f;      // Kd phase: drop <= threshold * 1.01
    g_config.fine_kp_max_factor = 1.003f;       // Kp phase: target <= drop <= target * 1.003

    // Clear session
    memset(&g_session, 0, sizeof(ai_tuning_session_t));
    g_session.state = AI_TUNING_IDLE;

    // Load config from EEPROM (user settings)
    load_config_from_eeprom();

    // Load ML history from flash (runtime learning data)
    load_history_from_flash();

    // Register EEPROM save handler
    eeprom_register_handler(save_config_to_eeprom);

    g_initialized = true;
}

// =============================================================================
// Config Accessor
// =============================================================================

ai_tuning_config_t* ai_tuning_get_config(void) {
    return &g_config;
}

// =============================================================================
// Tuning Session Start
// =============================================================================

bool ai_tuning_start(profile_t* profile) {
    if (!g_initialized) {
        ai_tuning_init();
    }

    if (g_ai_tuning_mutex != NULL) {
        if (xSemaphoreTakeRecursive(g_ai_tuning_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return false;
        }
    }

    if (profile == NULL) {
        if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
        return false;
    }

    // Save original profile values for restore on cancel
    g_orig_coarse_kp = profile->coarse_kp;
    g_orig_coarse_kd = profile->coarse_kd;
    g_orig_fine_kp = profile->fine_kp;
    g_orig_fine_kd = profile->fine_kd;

    // Initialize session
    memset(&g_session, 0, sizeof(ai_tuning_session_t));
    g_session.state = AI_TUNING_PHASE_1_COARSE;
    g_session.target_profile = profile;
    g_session.drops_completed = 0;
    g_session.max_drops_allowed = 30;
    g_session.phase2_start_idx = 0;

    // Get profile index for ML suggestions
    uint8_t profile_idx = 0;
    for (int i = 0; i < MAX_PROFILE_CNT; i++) {
        if (profile_select(i) == profile) {
            profile_idx = i;
            break;
        }
    }

    // Start from suggestions or zero
    float suggested_coarse_kp, suggested_coarse_kd;
    float suggested_fine_kp, suggested_fine_kd;
    bool have_suggestions = ai_tuning_get_suggestions(profile_idx,
                                                       &suggested_coarse_kp, &suggested_coarse_kd,
                                                       &suggested_fine_kp, &suggested_fine_kd);

    if (have_suggestions) {
        g_session.coarse_kp_best = suggested_coarse_kp * 0.7f;
        g_session.coarse_kd_best = suggested_coarse_kd * 0.7f;
        g_session.fine_kp_best = suggested_fine_kp * 0.7f;
        g_session.fine_kd_best = suggested_fine_kd * 0.7f;
    } else {
        g_session.coarse_kp_best = g_config.coarse_kp_min;
        g_session.coarse_kd_best = g_config.coarse_kd_min;
        g_session.fine_kp_best = g_config.fine_kp_min;
        g_session.fine_kd_best = g_config.fine_kd_min;
    }

    g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;
    g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;

    // Start at midpoint for binary search
    g_session.coarse_kp_best = (g_config.coarse_kp_min + g_config.coarse_kp_max) / 2.0f;
    g_session.coarse_kd_best = (g_config.coarse_kd_min + g_config.coarse_kd_max) / 2.0f;
    g_session.fine_kp_best = (g_config.fine_kp_min + g_config.fine_kp_max) / 2.0f;
    g_session.fine_kd_best = (g_config.fine_kd_min + g_config.fine_kd_max) / 2.0f;

    g_coarse_kp_step = (g_config.coarse_kp_max - g_config.coarse_kp_min) / 2.0f;
    g_coarse_kd_step = (g_config.coarse_kd_max - g_config.coarse_kd_min) / 2.0f;
    g_fine_kp_step = (g_config.fine_kp_max - g_config.fine_kp_min) / 2.0f;
    g_fine_kd_step = (g_config.fine_kd_max - g_config.fine_kd_min) / 2.0f;

    g_coarse_had_overthrow = false;
    g_fine_had_overthrow = false;

    if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
    return true;
}

// =============================================================================
// Get Next Parameters
// =============================================================================

bool ai_tuning_get_next_params(float* coarse_kp, float* coarse_kd,
                                 float* fine_kp, float* fine_kd) {
    if (g_ai_tuning_mutex != NULL) {
        if (xSemaphoreTakeRecursive(g_ai_tuning_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            return false;
        }
    }

    bool result = false;

    if (g_session.state == AI_TUNING_PHASE_1_COARSE) {
        *coarse_kp = g_session.coarse_kp_best;
        *coarse_kd = g_session.coarse_kd_best;
        *fine_kp = 0.0f;
        *fine_kd = 0.0f;
        result = true;
    }
    else if (g_session.state == AI_TUNING_PHASE_2_FINE) {
        *coarse_kp = g_session.recommended_coarse_kp;
        *coarse_kd = g_session.recommended_coarse_kd;
        *fine_kp = g_session.fine_kp_best;
        *fine_kd = g_session.fine_kd_best;
        result = true;
    }

    if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
    return result;
}

// =============================================================================
// Record Drop Telemetry
// =============================================================================

bool ai_tuning_record_drop(const ai_drop_telemetry_t* telemetry) {
    if (g_ai_tuning_mutex != NULL) {
        if (xSemaphoreTakeRecursive(g_ai_tuning_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            return false;
        }
    }

    if (!ai_tuning_is_active() || telemetry == NULL) {
        if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
        return false;
    }

    if (g_session.drops_completed >= g_session.max_drops_allowed) {
        g_session.state = AI_TUNING_ERROR;
        snprintf(g_session.error_message, sizeof(g_session.error_message),
                 "Did not converge in %d drops", g_session.max_drops_allowed);
        g_session.recommended_coarse_kp = g_session.coarse_kp_best;
        g_session.recommended_coarse_kd = g_session.coarse_kd_best;
        g_session.recommended_fine_kp = g_session.fine_kp_best;
        g_session.recommended_fine_kd = g_session.fine_kd_best;
        if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
        return false;
    }

    // Store telemetry in circular buffer
    uint8_t idx = g_session.drop_write_idx;
    memcpy(&g_session.drops[idx], telemetry, sizeof(ai_drop_telemetry_t));
    g_session.drop_write_idx = (g_session.drop_write_idx + 1) % AI_TUNING_DROP_BUF_SIZE;
    g_session.drops_completed++;

    // Dispatch to correct phase handler
    if (g_session.state == AI_TUNING_PHASE_1_COARSE) {
        calculate_next_params_phase1(telemetry);
    } else if (g_session.state == AI_TUNING_PHASE_2_FINE) {
        calculate_next_params_phase2(telemetry);
    }

    if (g_ai_tuning_mutex != NULL) xSemaphoreGiveRecursive(g_ai_tuning_mutex);
    return true;
}

// =============================================================================
// Phase 1: Tune Coarse Trickler (Binary Search)
// 1. Increase Kp until threshold <= drop <= threshold * 1.02
// 2. Increase Kd until drop <= threshold * 1.02
//    If time > target, bump Kp by step and continue Kd
// =============================================================================

static void calculate_next_params_phase1(const ai_drop_telemetry_t* drop) {
    float coarse_stop_threshold = charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold;
    float target_time_ms = (float)charge_mode_config.eeprom_charge_mode_data.coarse_time_target_ms;

    // "threshold" = coarse stop point (where coarse should stop)
    float threshold = drop->target_weight - coarse_stop_threshold;
    float final_weight = drop->final_weight;

    // Kp acceptance bounds: threshold <= drop <= target / 1.015
    float accept_min = threshold;
    float accept_max = drop->target_weight / 1.015f;
    // Kd acceptance: drop <= target - (threshold * 0.02)
    float kd_accept_max = drop->target_weight - (threshold * 0.02f);

    bool time_ok = (drop->coarse_time_ms <= target_time_ms);

    // Kp phase: change Kp until threshold <= drop <= target / 1.015
    if (g_coarse_tuning_phase == TUNING_PHASE_ADAPTIVE_KP) {
        // Halve step first (binary search)
        g_coarse_kp_step /= 2.0f;

        if (final_weight >= accept_min && final_weight <= accept_max) {
            // In range - move to Kd phase
            g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            g_session.coarse_kd_best = (g_config.coarse_kd_min + g_config.coarse_kd_max) / 2.0f;
            g_coarse_kd_step = (g_config.coarse_kd_max - g_config.coarse_kd_min) / 2.0f;
        } else if (final_weight > accept_max) {
            // Too high - decrease Kp
            g_session.coarse_kp_best -= g_coarse_kp_step;
        } else if (final_weight < accept_min) {
            // Too low - increase Kp
            g_session.coarse_kp_best += g_coarse_kp_step;
        }

        // Step too small - move to Kd phase anyway
        if (g_coarse_kp_step < COARSE_MIN_STEP) {
            g_coarse_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            g_session.coarse_kd_best = (g_config.coarse_kd_min + g_config.coarse_kd_max) / 2.0f;
            g_coarse_kd_step = (g_config.coarse_kd_max - g_config.coarse_kd_min) / 2.0f;
        }
    }
    // Kd phase: change Kd until drop <= target - (threshold * 0.02)
    // If time > target, increase Kp by step, then go to Fine
    else if (g_coarse_tuning_phase == TUNING_PHASE_ADAPTIVE_KD) {
        // Halve step first (binary search)
        g_coarse_kd_step /= 2.0f;

        // If time bad, bump Kp
        if (!time_ok) {
            g_session.coarse_kp_best += g_coarse_kp_step;
        }

        if (final_weight <= kd_accept_max) {
            // In range - done with coarse, go directly to Fine
            g_session.recommended_coarse_kp = g_session.coarse_kp_best;
            g_session.recommended_coarse_kd = g_session.coarse_kd_best;
            g_session.phase2_start_idx = g_session.drops_completed;
            g_session.state = AI_TUNING_PHASE_2_FINE;
            g_session.fine_kp_best = (g_config.fine_kp_min + g_config.fine_kp_max) / 2.0f;
            g_session.fine_kd_best = (g_config.fine_kd_min + g_config.fine_kd_max) / 2.0f;
            g_fine_kp_step = (g_config.fine_kp_max - g_config.fine_kp_min) / 2.0f;
            g_fine_kd_step = (g_config.fine_kd_max - g_config.fine_kd_min) / 2.0f;
            g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;
        } else {
            // Too high - increase Kd
            g_session.coarse_kd_best += g_coarse_kd_step;
        }

        // Step too small - move to Fine
        if (g_coarse_kd_step < COARSE_MIN_STEP) {
            g_session.recommended_coarse_kp = g_session.coarse_kp_best;
            g_session.recommended_coarse_kd = g_session.coarse_kd_best;
            g_session.phase2_start_idx = g_session.drops_completed;
            g_session.state = AI_TUNING_PHASE_2_FINE;
            g_session.fine_kp_best = (g_config.fine_kp_min + g_config.fine_kp_max) / 2.0f;
            g_session.fine_kd_best = (g_config.fine_kd_min + g_config.fine_kd_max) / 2.0f;
            g_fine_kp_step = (g_config.fine_kp_max - g_config.fine_kp_min) / 2.0f;
            g_fine_kd_step = (g_config.fine_kd_max - g_config.fine_kd_min) / 2.0f;
            g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KP;
        }
    }

    // Clamp to valid range
    g_session.coarse_kp_best = fminf(fmaxf(g_session.coarse_kp_best, g_config.coarse_kp_min), g_config.coarse_kp_max);
    g_session.coarse_kd_best = fminf(fmaxf(g_session.coarse_kd_best, g_config.coarse_kd_min), g_config.coarse_kd_max);
}

// =============================================================================
// Phase 2: Tune Fine Trickler (Binary Search)
// 1. Increase Kp until target <= drop <= target * 1.003
// 2. Increase Kd until drop = target ± 0.05gn
// =============================================================================

static void calculate_next_params_phase2(const ai_drop_telemetry_t* drop) {
    float target = drop->target_weight;
    float final_weight = drop->final_weight;
    float target_time_ms = (float)charge_mode_config.eeprom_charge_mode_data.total_time_target_ms;
    float noise_margin = g_config.noise_margin;  // 0.05gn

    // Acceptance bounds:
    // Kp: target <= drop <= target * 1.003
    // Kd: drop = target ± 0.05gn
    float kp_max = target * g_config.fine_kp_max_factor;  // target * 1.003

    bool time_ok = (drop->total_time_ms <= target_time_ms);

    // Kp phase: change Kp until target <= drop <= target * 1.003
    if (g_fine_tuning_phase == TUNING_PHASE_ADAPTIVE_KP) {
        // Halve step first (binary search)
        g_fine_kp_step /= 2.0f;

        if (final_weight >= target && final_weight <= kp_max) {
            // In range - move to Kd phase
            g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            g_session.fine_kd_best = (g_config.fine_kd_min + g_config.fine_kd_max) / 2.0f;
            g_fine_kd_step = (g_config.fine_kd_max - g_config.fine_kd_min) / 2.0f;
        } else if (final_weight > kp_max) {
            // Too high - decrease Kp
            g_session.fine_kp_best -= g_fine_kp_step;
        } else if (final_weight < target) {
            // Too low - increase Kp
            g_session.fine_kp_best += g_fine_kp_step;
        }

        // Step too small - move to Kd phase anyway
        if (g_fine_kp_step < FINE_MIN_STEP) {
            g_fine_tuning_phase = TUNING_PHASE_ADAPTIVE_KD;
            g_session.fine_kd_best = (g_config.fine_kd_min + g_config.fine_kd_max) / 2.0f;
            g_fine_kd_step = (g_config.fine_kd_max - g_config.fine_kd_min) / 2.0f;
        }
    }
    // Kd phase: change Kd until drop = target ± 0.05gn
    // If time > target, increase Kp by step
    else if (g_fine_tuning_phase == TUNING_PHASE_ADAPTIVE_KD) {
        // Halve step first (binary search)
        g_fine_kd_step /= 2.0f;

        // If time bad, bump Kp
        if (!time_ok) {
            g_session.fine_kp_best += g_fine_kp_step;
        }

        float error = fabsf(final_weight - target);

        if (error <= noise_margin) {
            // Within ±0.05gn of target - done!
            g_session.recommended_fine_kp = g_session.fine_kp_best;
            g_session.recommended_fine_kd = g_session.fine_kd_best;
            g_session.state = AI_TUNING_COMPLETE;
        } else if (final_weight > target + noise_margin) {
            // Too high - increase Kd
            g_session.fine_kd_best += g_fine_kd_step;
        } else if (final_weight < target - noise_margin) {
            // Too low - decrease Kd
            g_session.fine_kd_best -= g_fine_kd_step;
        }

        // Step too small - done
        if (g_fine_kd_step < FINE_MIN_STEP) {
            g_session.recommended_fine_kp = g_session.fine_kp_best;
            g_session.recommended_fine_kd = g_session.fine_kd_best;
            g_session.state = AI_TUNING_COMPLETE;
        }
    }

    // Clamp to valid range
    g_session.fine_kp_best = fminf(fmaxf(g_session.fine_kp_best, g_config.fine_kp_min), g_config.fine_kp_max);
    g_session.fine_kd_best = fminf(fmaxf(g_session.fine_kd_best, g_config.fine_kd_min), g_config.fine_kd_max);
}

// =============================================================================
// Finalize Recommendations
// =============================================================================

static void finalize_recommendations(void) {
    g_session.state = AI_TUNING_COMPLETE;

    float total_overthrow = 0.0f;
    float total_time = 0.0f;
    int count = g_session.drops_completed;
    int buf_count = (count < AI_TUNING_DROP_BUF_SIZE) ? count : AI_TUNING_DROP_BUF_SIZE;

    for (int i = 0; i < buf_count; i++) {
        total_overthrow += g_session.drops[i].overthrow;
        total_time += g_session.drops[i].total_time_ms;
    }

    if (buf_count > 0) {
        g_session.avg_overthrow = total_overthrow / buf_count;
        g_session.avg_total_time = total_time / buf_count;
    }
}

// =============================================================================
// Session State Queries
// =============================================================================

bool ai_tuning_is_complete(void) {
    return g_session.state == AI_TUNING_COMPLETE;
}

ai_tuning_session_t* ai_tuning_get_session(void) {
    return &g_session;
}

void ai_tuning_get_session_copy(ai_tuning_session_t* out) {
    if (g_ai_tuning_mutex != NULL) {
        xSemaphoreTakeRecursive(g_ai_tuning_mutex, pdMS_TO_TICKS(50));
    }
    memcpy(out, &g_session, sizeof(ai_tuning_session_t));
    if (g_ai_tuning_mutex != NULL) {
        xSemaphoreGiveRecursive(g_ai_tuning_mutex);
    }
}

void ai_tuning_get_history_copy(ai_tuning_history_t* out) {
    if (g_ai_tuning_mutex != NULL) {
        xSemaphoreTakeRecursive(g_ai_tuning_mutex, pdMS_TO_TICKS(50));
    }
    memcpy(out, &g_history, sizeof(ai_tuning_history_t));
    if (g_ai_tuning_mutex != NULL) {
        xSemaphoreGiveRecursive(g_ai_tuning_mutex);
    }
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

    g_session.target_profile->coarse_kp = g_session.recommended_coarse_kp;
    g_session.target_profile->coarse_kd = g_session.recommended_coarse_kd;
    g_session.target_profile->fine_kp = g_session.recommended_fine_kp;
    g_session.target_profile->fine_kd = g_session.recommended_fine_kd;

    g_session.state = AI_TUNING_IDLE;

    return true;
}

void ai_tuning_cancel(void) {
    bool was_active = ai_tuning_is_active();

    // Restore original profile values if we have a target profile
    if (g_session.target_profile != NULL) {
        g_session.target_profile->coarse_kp = g_orig_coarse_kp;
        g_session.target_profile->coarse_kd = g_orig_coarse_kd;
        g_session.target_profile->fine_kp = g_orig_fine_kp;
        g_session.target_profile->fine_kd = g_orig_fine_kd;
    }

    // Clear session state
    g_session.state = AI_TUNING_IDLE;
    memset(&g_session, 0, sizeof(ai_tuning_session_t));

    // Exit charge mode so the trickler stops (only if was actually running)
    if (was_active) {
        charge_mode_config.charge_mode_state = CHARGE_MODE_EXIT;
        ButtonEncoderEvent_t button_event = BUTTON_RST_PRESSED;
        xQueueSend(encoder_event_queue, &button_event, 0);
    }
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
    if (g_session.max_drops_allowed == 0) {
        return 0;
    }
    return (100 * g_session.drops_completed) / g_session.max_drops_allowed;
}

// =============================================================================
// Flash Persistence
// =============================================================================

static void load_history_from_flash(void) {
    if (g_history_loaded) {
        return;
    }

    bool ok = flash_ml_history_read((uint8_t*)&g_history, sizeof(ai_tuning_history_t));

    if (!ok || g_history.revision != AI_TUNING_HISTORY_REV) {
        memset(&g_history, 0, sizeof(ai_tuning_history_t));
        g_history.revision = AI_TUNING_HISTORY_REV;
    }

    g_history_loaded = true;
}

static void save_history_to_flash(void) {
    flash_ml_history_write((const uint8_t*)&g_history, sizeof(ai_tuning_history_t));
}

// =============================================================================
// EEPROM Config Persistence
// =============================================================================

static void load_config_from_eeprom(void) {
    if (g_config_loaded) {
        return;
    }

    bool ok = eeprom_read(EEPROM_AI_TUNING_CONFIG_BASE_ADDR,
                          (uint8_t*)&g_config_eeprom,
                          sizeof(ai_tuning_config_eeprom_t));

    if (!ok || g_config_eeprom.revision != AI_TUNING_CONFIG_REV) {
        // Keep defaults set in ai_tuning_init()
    } else {
        // Apply EEPROM config to runtime config
        g_config.coarse_kp_min = g_config_eeprom.coarse_kp_min;
        g_config.coarse_kp_max = g_config_eeprom.coarse_kp_max;
        g_config.coarse_kd_min = g_config_eeprom.coarse_kd_min;
        g_config.coarse_kd_max = g_config_eeprom.coarse_kd_max;
        g_config.fine_kp_min = g_config_eeprom.fine_kp_min;
        g_config.fine_kp_max = g_config_eeprom.fine_kp_max;
        g_config.fine_kd_min = g_config_eeprom.fine_kd_min;
        g_config.fine_kd_max = g_config_eeprom.fine_kd_max;
        g_config.noise_margin = g_config_eeprom.noise_margin;
    }

    g_config_loaded = true;
}

static bool save_config_to_eeprom(void) {
    // Copy runtime config to EEPROM structure
    g_config_eeprom.revision = AI_TUNING_CONFIG_REV;
    g_config_eeprom.coarse_kp_min = g_config.coarse_kp_min;
    g_config_eeprom.coarse_kp_max = g_config.coarse_kp_max;
    g_config_eeprom.coarse_kd_min = g_config.coarse_kd_min;
    g_config_eeprom.coarse_kd_max = g_config.coarse_kd_max;
    g_config_eeprom.fine_kp_min = g_config.fine_kp_min;
    g_config_eeprom.fine_kp_max = g_config.fine_kp_max;
    g_config_eeprom.fine_kd_min = g_config.fine_kd_min;
    g_config_eeprom.fine_kd_max = g_config.fine_kd_max;
    g_config_eeprom.noise_margin = g_config.noise_margin;

    bool ok = eeprom_write(EEPROM_AI_TUNING_CONFIG_BASE_ADDR,
                           (uint8_t*)&g_config_eeprom,
                           sizeof(ai_tuning_config_eeprom_t));

    return ok;
}

void ai_tuning_save_config(void) {
    save_config_to_eeprom();
}

ai_tuning_history_t* ai_tuning_get_history(void) {
    load_history_from_flash();
    return &g_history;
}

void ai_tuning_record_charge(uint8_t profile_idx,
                              float coarse_kp, float coarse_kd,
                              float fine_kp, float fine_kd,
                              float overthrow,
                              float coarse_time_ms, float fine_time_ms) {
    load_history_from_flash();

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

    g_history.drops[g_history.next_idx] = record;
    g_history.next_idx = (g_history.next_idx + 1) % AI_TUNING_HISTORY_SIZE;
    if (g_history.count < AI_TUNING_HISTORY_SIZE) {
        g_history.count++;
    }

    if (g_history.count >= 3) {
        ai_tuning_calculate_refinements(profile_idx);
    }

    save_history_to_flash();
}

void ai_tuning_calculate_refinements(uint8_t profile_idx) {
    load_history_from_flash();

    if (g_history.count < 3) {
        g_history.has_suggestions = false;
        return;
    }

    float avg_overthrow = 0.0f;
    float avg_fine_kp = 0.0f, avg_fine_kd = 0.0f;

    for (int i = 0; i < g_history.count; i++) {
        ai_drop_record_t* d = &g_history.drops[i];
        avg_overthrow += d->overthrow;
        avg_fine_kp += d->fine_kp;
        avg_fine_kd += d->fine_kd;
    }

    float inv_count = 1.0f / g_history.count;
    avg_overthrow *= inv_count;
    avg_fine_kp *= inv_count;
    avg_fine_kd *= inv_count;

    // ML only adjusts fine parameters - coarse stays unchanged
    float fine_kp_adj = 0.0f, fine_kd_adj = 0.0f;

    if (avg_overthrow > g_config.noise_margin) {
        // Overthrow: increase Kd to slow down
        fine_kd_adj = 0.1f;
    } else if (avg_overthrow < -g_config.noise_margin) {
        // Underthrow: increase Kp to speed up
        fine_kp_adj = 0.1f;
    }

    // Coarse values unchanged (set to 0 to indicate no suggestion)
    g_history.suggested_coarse_kp = 0.0f;
    g_history.suggested_coarse_kd = 0.0f;
    g_history.suggested_fine_kp = fminf(fmaxf(avg_fine_kp + fine_kp_adj, g_config.fine_kp_min), g_config.fine_kp_max);
    g_history.suggested_fine_kd = fminf(fmaxf(avg_fine_kd + fine_kd_adj, g_config.fine_kd_min), g_config.fine_kd_max);
    g_history.has_suggestions = true;
}

bool ai_tuning_get_refined_params(float* coarse_kp, float* coarse_kd,
                                   float* fine_kp, float* fine_kd) {
    load_history_from_flash();

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
    load_history_from_flash();

    if (g_history.count < 3) {
        return false;
    }

    float sum_coarse_kp = 0, sum_coarse_kd = 0;
    float sum_fine_kp = 0, sum_fine_kd = 0;
    int count = 0;

    for (int i = 0; i < g_history.count; i++) {
        ai_drop_record_t* d = &g_history.drops[i];
        if (profile_idx == 0xFF || d->profile_idx == profile_idx) {
            sum_coarse_kp += d->coarse_kp;
            sum_coarse_kd += d->coarse_kd;
            sum_fine_kp += d->fine_kp;
            sum_fine_kd += d->fine_kd;
            count++;
        }
    }

    if (count < 3) {
        if (g_history.has_suggestions) {
            *coarse_kp = g_history.suggested_coarse_kp;
            *coarse_kd = g_history.suggested_coarse_kd;
            *fine_kp = g_history.suggested_fine_kp;
            *fine_kd = g_history.suggested_fine_kd;
            return true;
        }
        return false;
    }

    float inv_count = 1.0f / count;
    *coarse_kp = sum_coarse_kp * inv_count;
    *coarse_kd = sum_coarse_kd * inv_count;
    *fine_kp = sum_fine_kp * inv_count;
    *fine_kd = sum_fine_kd * inv_count;

    return true;
}

bool ai_tuning_apply_refined_params(uint8_t profile_idx) {
    load_history_from_flash();

    if (!g_history.has_suggestions) {
        return false;
    }

    profile_t* profile = profile_select(profile_idx);
    if (!profile) {
        return false;
    }

    // ML only adjusts fine parameters - coarse stays unchanged
    profile->fine_kp = g_history.suggested_fine_kp;
    profile->fine_kd = g_history.suggested_fine_kd;

    profile_data_save();

    g_history.count = 0;
    g_history.next_idx = 0;
    g_history.has_suggestions = false;
    save_history_to_flash();

    return true;
}

void ai_tuning_clear_history(void) {
    memset(&g_history, 0, sizeof(ai_tuning_history_t));
    g_history.revision = AI_TUNING_HISTORY_REV;
    save_history_to_flash();
}

float ai_tuning_get_scale_compensation(void) {
    return 0.0f;  // Disabled - was over-engineering
}
