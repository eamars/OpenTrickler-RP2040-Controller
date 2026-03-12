#ifndef AI_TUNING_H_
#define AI_TUNING_H_

#include <stdint.h>
#include <stdbool.h>
#include "profile.h"

#define AI_TUNING_HISTORY_SIZE 10
#define AI_TUNING_HISTORY_REV 4  // Bumped: added scale_latency_compensation
#define AI_TUNING_DROP_BUF_SIZE 10
#define AI_TUNING_CONFIG_REV 2   // EEPROM config revision (v2: removed coarse_overthrow_max_percent)

/**
 * AI Tuning System
 *
 * Automatically tunes Kp and Kd parameters for both coarse and fine tricklers.
 * Uses adaptive step-halving algorithm with GP refinement.
 */

typedef enum {
    AI_TUNING_IDLE = 0,
    AI_TUNING_PHASE_1_COARSE,
    AI_TUNING_PHASE_2_FINE,
    AI_TUNING_COMPLETE,
    AI_TUNING_ERROR
} ai_tuning_state_t;

typedef enum {
    AI_MOTOR_MODE_NORMAL = 0,
    AI_MOTOR_MODE_COARSE_ONLY,
    AI_MOTOR_MODE_FINE_ONLY
} ai_motor_mode_t;

typedef struct {
    uint8_t drop_number;
    float coarse_time_ms;
    float fine_time_ms;
    float total_time_ms;
    float final_weight;
    float target_weight;
    float overthrow;
    float overthrow_percent;
    float coarse_kp_used;
    float coarse_kd_used;
    float fine_kp_used;
    float fine_kd_used;
} ai_drop_telemetry_t;

typedef struct {
    ai_tuning_state_t state;
    profile_t* target_profile;
    uint8_t drops_completed;
    uint8_t max_drops_allowed;
    uint8_t phase2_start_idx;

    // Circular buffer for last 10 drops
    ai_drop_telemetry_t drops[AI_TUNING_DROP_BUF_SIZE];
    uint8_t drop_write_idx;

    float coarse_kp_best;
    float coarse_kd_best;
    float fine_kp_best;
    float fine_kd_best;

    float recommended_coarse_kp;
    float recommended_coarse_kd;
    float recommended_fine_kp;
    float recommended_fine_kd;

    float avg_overthrow;
    float avg_total_time;

    char error_message[64];
} ai_tuning_session_t;

typedef struct {
    float max_overthrow_percent;

    float coarse_kp_min;
    float coarse_kp_max;
    float coarse_kd_min;
    float coarse_kd_max;

    float fine_kp_min;
    float fine_kp_max;
    float fine_kd_min;
    float fine_kd_max;

    float noise_margin;

    // Tuning acceptance factors
    float coarse_kp_max_factor;   // Kp phase: threshold <= drop <= threshold * this (1.2)
    float coarse_kd_max_factor;   // Kd phase: drop <= threshold * this (1.1)
    float fine_kp_max_factor;     // Kp phase: target <= drop <= target * this (1.02)
} ai_tuning_config_t;

typedef struct {
    float coarse_kp;
    float coarse_kd;
    float fine_kp;
    float fine_kd;
    float overthrow;
    float coarse_time_ms;
    float fine_time_ms;
    float total_time_ms;
    uint8_t profile_idx;
} ai_drop_record_t;

// AI tuning config stored in EEPROM (separate from ML history)
typedef struct {
    uint16_t revision;
    float coarse_kp_min;
    float coarse_kp_max;
    float coarse_kd_min;
    float coarse_kd_max;
    float fine_kp_min;
    float fine_kp_max;
    float fine_kd_min;
    float fine_kd_max;
    float noise_margin;
} __attribute__((packed)) ai_tuning_config_eeprom_t;

// ML history stored in flash (runtime learning data)
typedef struct {
    uint16_t revision;
    uint8_t count;
    uint8_t next_idx;
    ai_drop_record_t drops[AI_TUNING_HISTORY_SIZE];
    float suggested_coarse_kp;
    float suggested_coarse_kd;
    float suggested_fine_kp;
    float suggested_fine_kd;
    bool has_suggestions;
    float scale_latency_compensation;  // Learned overthrow compensation (grains)
} ai_tuning_history_t;

#ifdef __cplusplus
extern "C" {
#endif

void ai_tuning_init(void);
ai_tuning_config_t* ai_tuning_get_config(void);

bool ai_tuning_start(profile_t* profile);
bool ai_tuning_record_drop(const ai_drop_telemetry_t* telemetry);
bool ai_tuning_get_next_params(float* coarse_kp, float* coarse_kd, float* fine_kp, float* fine_kd);
bool ai_tuning_is_complete(void);
ai_tuning_session_t* ai_tuning_get_session(void);
void ai_tuning_get_session_copy(ai_tuning_session_t* out);
void ai_tuning_get_history_copy(ai_tuning_history_t* out);
bool ai_tuning_get_recommended_params(float* coarse_kp, float* coarse_kd, float* fine_kp, float* fine_kd);
bool ai_tuning_apply_params(void);
void ai_tuning_cancel(void);
bool ai_tuning_is_active(void);
ai_motor_mode_t ai_tuning_get_motor_mode(void);
uint8_t ai_tuning_get_progress_percent(void);

// ML Learning
ai_tuning_history_t* ai_tuning_get_history(void);
void ai_tuning_record_charge(uint8_t profile_idx, float coarse_kp, float coarse_kd,
                              float fine_kp, float fine_kd, float overthrow,
                              float coarse_time_ms, float fine_time_ms);
void ai_tuning_calculate_refinements(uint8_t profile_idx);
bool ai_tuning_get_refined_params(float* coarse_kp, float* coarse_kd, float* fine_kp, float* fine_kd);
bool ai_tuning_get_suggestions(uint8_t profile_idx, float* coarse_kp, float* coarse_kd,
                                float* fine_kp, float* fine_kd);
bool ai_tuning_apply_refined_params(uint8_t profile_idx);
void ai_tuning_clear_history(void);
void ai_tuning_save_config(void);
float ai_tuning_get_scale_compensation(void);  // Get learned scale latency compensation

#ifdef __cplusplus
}
#endif

#endif  // AI_TUNING_H_
