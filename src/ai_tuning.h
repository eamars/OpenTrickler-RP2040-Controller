#ifndef AI_TUNING_H_
#define AI_TUNING_H_

#include <stdint.h>
#include <stdbool.h>
#include "profile.h"

#define AI_TUNING_HISTORY_SIZE 10
#define AI_TUNING_HISTORY_REV 1

/**
 * HYBRID PID Auto-Tuning System (Adaptive + Gaussian Process)
 *
 * Automatically tunes Kp and Kd parameters for both coarse and fine tricklers.
 * Ki is kept at 0 for both motors.
 *
 * PARAMETER RANGES:
 *   Coarse: Kp 0-1, Kd 0-1  (gentle, large motor)
 *   Fine:   Kp 0-10, Kd 0-10 (aggressive, small motor)
 *
 * HYBRID ALGORITHM:
 *
 * Phase 1: Tune Coarse Trickler (0-1 range)
 *   Step A: Adaptive - increase Kp until overthrow, tune Kd
 *   Step B: GP Refinement - use Gaussian Process to find optimal Kp×Kd combo
 *   Goal: coarse time < 10 seconds
 *
 * Phase 2: Tune Fine Trickler (0-10 range)
 *   Step A: Adaptive - increase Kp until overthrow, tune Kd
 *   Step B: GP Refinement - find optimal Kp×Kd combo
 *   Goal: total time < 15 seconds, overthrow < 1/15 (6.67%)
 *
 * WHY HYBRID:
 *   - Adaptive: Fast convergence to "good" region (~5-8 drops)
 *   - GP: Finds true optimum by considering Kp×Kd interaction (~5 drops)
 *   - Total: ~15 drops for better results than adaptive alone
 *
 * RP2350 OPTIMIZATION:
 *   - GP uses hardware FPU for fast exp()/sqrt() in kernel calculations
 *   - Matrix operations leverage Cortex-M33 performance
 *
 * Usage:
 * 1. ai_tuning_start(profile) - Begin tuning session
 * 2. Charge mode calls ai_tuning_record_drop() after each drop
 * 3. ai_tuning_get_recommended_params() when complete
 * 4. User confirms and applies parameters
 */

// Tuning state machine
typedef enum {
    AI_TUNING_IDLE = 0,
    AI_TUNING_PHASE_1_COARSE,     // Tune coarse trickler (fine OFF)
    AI_TUNING_PHASE_2_FINE,       // Tune fine trickler (coarse OFF, precharge to threshold first)
    AI_TUNING_COMPLETE,           // Tuning finished, awaiting confirmation
    AI_TUNING_ERROR               // Error occurred during tuning
} ai_tuning_state_t;

// Motor control mode during tuning
typedef enum {
    AI_MOTOR_MODE_NORMAL = 0,     // Normal charge: coarse then fine
    AI_MOTOR_MODE_COARSE_ONLY,    // Phase 1: Only coarse runs, fine OFF
    AI_MOTOR_MODE_FINE_ONLY       // Phase 2: Precharge, then only fine runs
} ai_motor_mode_t;

// Tuning speed mode
typedef enum {
    AI_TUNING_MODE_QUICK = 0,     // 0.1 increments - faster but less precise
    AI_TUNING_MODE_FINE           // 0.05 increments - slower but more precise
} ai_tuning_mode_t;

// Drop telemetry data collected during each drop
typedef struct {
    uint8_t drop_number;          // 1-10

    // Timing
    float coarse_time_ms;         // Time spent in coarse trickling
    float fine_time_ms;           // Time spent in fine trickling
    float total_time_ms;          // Total drop time

    // Accuracy
    float final_weight;           // Final weight achieved
    float target_weight;          // Target weight
    float overthrow;              // Amount of overthrow (negative = underthrow)
    float overthrow_percent;      // Overthrow as percentage of target

    // PID values used for this drop
    float coarse_kp_used;
    float coarse_kd_used;
    float fine_kp_used;
    float fine_kd_used;

    // Quality metrics
    float accuracy_score;         // 0-100, higher is better
    float speed_score;            // 0-100, higher is better
    float overall_score;          // Weighted combination
} ai_drop_telemetry_t;

// AI tuning session state
typedef struct {
    ai_tuning_state_t state;

    profile_t* target_profile;    // Profile being tuned

    // Progress
    uint8_t drops_completed;      // Current drop count
    uint8_t total_drops_target;   // Estimate (adaptive)
    uint8_t max_drops_allowed;    // Maximum drops (safety limit: 50)
    uint8_t phase2_start_idx;     // Drop index where Phase 2 started

    // Telemetry history
    ai_drop_telemetry_t drops[50];

    // Current Kp/Kd being tested
    float coarse_kp_best;
    float coarse_kd_best;
    float fine_kp_best;
    float fine_kd_best;

    // Recommended final values (set when phase completes)
    float recommended_coarse_kp;
    float recommended_coarse_kd;
    float recommended_fine_kp;
    float recommended_fine_kd;

    // Statistics (calculated at completion)
    float avg_overthrow;
    float avg_total_time;
    float consistency_score;

    // Error handling
    char error_message[64];
} ai_tuning_session_t;

// Configuration
typedef struct {
    // Target performance goals
    float max_overthrow_percent;      // Maximum acceptable overthrow (default: 6.67% = 1/15)
    float target_coarse_time_ms;      // Target coarse time (default: 10000ms)
    float target_total_time_ms;       // Target total time (default: 15000ms)

    // Coarse parameter limits (0-1 range for gentle, large motor)
    float coarse_kp_min;              // Min coarse Kp (default: 0.0)
    float coarse_kp_max;              // Max coarse Kp (default: 1.0)
    float coarse_kd_min;              // Min coarse Kd (default: 0.0)
    float coarse_kd_max;              // Max coarse Kd (default: 1.0)

    // Fine parameter limits (0-10 range for aggressive, small motor)
    float fine_kp_min;                // Min fine Kp (default: 0.0)
    float fine_kp_max;                // Max fine Kp (default: 10.0)
    float fine_kd_min;                // Min fine Kd (default: 0.0)
    float fine_kd_max;                // Max fine Kd (default: 10.0)

    // Tuning mode
    ai_tuning_mode_t tuning_mode;     // Quick or Fine mode

} ai_tuning_config_t;

// Drop record for ML learning (stored in EEPROM)
typedef struct {
    // PID values used
    float coarse_kp;
    float coarse_kd;
    float fine_kp;
    float fine_kd;
    // Results
    float overthrow;          // Positive = over, negative = under
    float coarse_time_ms;
    float fine_time_ms;
    float total_time_ms;
    uint8_t profile_idx;
} ai_drop_record_t;

// ML history storage (persisted to EEPROM)
typedef struct {
    uint16_t revision;
    uint8_t count;            // Number of valid records (0-10)
    uint8_t next_idx;         // Next write index (circular buffer)
    ai_drop_record_t drops[AI_TUNING_HISTORY_SIZE];
    // Suggested refined values (calculated from drops)
    float suggested_coarse_kp;
    float suggested_coarse_kd;
    float suggested_fine_kp;
    float suggested_fine_kd;
    bool has_suggestions;
} ai_tuning_history_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize AI tuning system with default configuration
 */
void ai_tuning_init(void);

/**
 * Get current tuning configuration
 */
ai_tuning_config_t* ai_tuning_get_config(void);

/**
 * Set tuning mode (quick or fine)
 * @param mode AI_TUNING_MODE_QUICK (0.1) or AI_TUNING_MODE_FINE (0.05)
 */
void ai_tuning_set_mode(ai_tuning_mode_t mode);

/**
 * Get current tuning mode
 */
ai_tuning_mode_t ai_tuning_get_mode(void);

/**
 * Start a new tuning session for the given profile
 * @param profile Profile to tune
 * @return true if session started successfully
 */
bool ai_tuning_start(profile_t* profile);

/**
 * Record telemetry data from a completed drop
 * Called by charge_mode after each drop completes
 * @param telemetry Drop telemetry data
 * @return true if recorded successfully
 */
bool ai_tuning_record_drop(const ai_drop_telemetry_t* telemetry);

/**
 * Get parameters to use for the next drop
 * Called by charge_mode before starting a drop
 * @param coarse_kp Output: Kp value for coarse trickler
 * @param coarse_kd Output: Kd value for coarse trickler
 * @param fine_kp Output: Kp value for fine trickler
 * @param fine_kd Output: Kd value for fine trickler
 * @return true if parameters are available
 */
bool ai_tuning_get_next_params(float* coarse_kp, float* coarse_kd,
                                 float* fine_kp, float* fine_kd);

/**
 * Check if tuning session is complete
 */
bool ai_tuning_is_complete(void);

/**
 * Get current tuning session state
 */
ai_tuning_session_t* ai_tuning_get_session(void);

/**
 * Get recommended parameters after tuning completes
 * @param coarse_kp Output: Recommended coarse Kp
 * @param coarse_kd Output: Recommended coarse Kd
 * @param fine_kp Output: Recommended fine Kp
 * @param fine_kd Output: Recommended fine Kd
 * @return true if recommendations available
 */
bool ai_tuning_get_recommended_params(float* coarse_kp, float* coarse_kd,
                                       float* fine_kp, float* fine_kd);

/**
 * Apply recommended parameters to the profile
 * Called after user confirms the recommendations
 * @return true if applied successfully
 */
bool ai_tuning_apply_params(void);

/**
 * Cancel current tuning session
 */
void ai_tuning_cancel(void);

/**
 * Check if tuning session is active
 */
bool ai_tuning_is_active(void);

/**
 * Get motor mode for current tuning phase
 * Returns AI_MOTOR_MODE_NORMAL if not tuning
 */
ai_motor_mode_t ai_tuning_get_motor_mode(void);

/**
 * Get progress percentage (0-100)
 */
uint8_t ai_tuning_get_progress_percent(void);

/**
 * Get ML history
 * @return Pointer to history structure
 */
ai_tuning_history_t* ai_tuning_get_history(void);

/**
 * Record a drop from normal charging for ML learning
 * Call this after every charge completes (not just during tuning)
 * @param profile_idx Profile index used
 * @param coarse_kp/kd PID values used for coarse
 * @param fine_kp/kd PID values used for fine
 * @param overthrow Amount over/under target
 * @param coarse_time_ms Time for coarse phase
 * @param fine_time_ms Time for fine phase
 */
void ai_tuning_record_charge(uint8_t profile_idx,
                              float coarse_kp, float coarse_kd,
                              float fine_kp, float fine_kd,
                              float overthrow,
                              float coarse_time_ms, float fine_time_ms);

/**
 * Calculate refined PID values from collected drop data
 * Analyzes last 10 drops and suggests improvements
 */
void ai_tuning_calculate_refinements(void);

/**
 * Get suggested refined PID values
 * @return true if suggestions available
 */
bool ai_tuning_get_refined_params(float* coarse_kp, float* coarse_kd,
                                   float* fine_kp, float* fine_kd);

/**
 * Get ML suggestions for a specific profile
 * Uses history to suggest starting values for tuning
 * @param profile_idx Profile index to get suggestions for
 * @return true if suggestions available
 */
bool ai_tuning_get_suggestions(uint8_t profile_idx,
                                float* coarse_kp, float* coarse_kd,
                                float* fine_kp, float* fine_kd);

/**
 * Apply refined PID values to a profile
 * @param profile_idx Profile to update
 * @return true if applied successfully
 */
bool ai_tuning_apply_refined_params(uint8_t profile_idx);

/**
 * Clear all ML history
 */
void ai_tuning_clear_history(void);

#ifdef __cplusplus
}
#endif

#endif  // AI_TUNING_H_
