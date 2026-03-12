#ifndef REST_AI_TUNING_H
#define REST_AI_TUNING_H

#include <stdbool.h>
#include "lwip/apps/fs.h"

/**
 * REST API Endpoints for AI Tuning
 *
 * Endpoints:
 * - POST /rest/ai_tuning_start  - Start AI tuning for a profile
 * - GET  /rest/ai_tuning_status - Get current tuning status and progress
 * - POST /rest/ai_tuning_apply  - Apply recommended parameters to profile
 * - POST /rest/ai_tuning_cancel - Cancel tuning in progress
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize AI tuning REST endpoints
 * Registers all endpoints with the REST handler
 *
 * @return true if initialization successful
 */
bool rest_ai_tuning_init(void);

/**
 * POST /rest/ai_tuning_start
 *
 * Start AI tuning session for specified profile.
 * This will automatically enter charge mode and begin tuning.
 *
 * Parameters:
 * - profile_idx: Profile index (0-7) [required]
 * - target: Target charge weight in grains (default: 30.0) [optional]
 *
 * Returns: JSON with success status, profile name, target_weight
 *
 * Example: /rest/ai_tuning_start?profile_idx=0&target=42.5
 */
bool http_rest_ai_tuning_start(struct fs_file *file, int num_params,
                                 char *params[], char *values[]);

/**
 * GET /rest/ai_tuning_status
 *
 * Returns JSON with:
 * - state: "idle", "phase1_coarse", "phase2_fine", "complete", "error"
 * - drops_completed: Number of drops completed
 * - progress_percent: Progress percentage (0-100)
 * - current_params: Current Kp/Kd being tested
 * - recommended_params: Recommended values (if complete)
 * - statistics: Performance statistics (avg_overthrow, avg_time)
 */
bool http_rest_ai_tuning_status(struct fs_file *file, int num_params,
                                  char *params[], char *values[]);

/**
 * POST /rest/ai_tuning_apply
 *
 * Apply recommended parameters to profile
 *
 * Returns: Success/error message
 */
bool http_rest_ai_tuning_apply(struct fs_file *file, int num_params,
                                 char *params[], char *values[]);

/**
 * POST /rest/ai_tuning_cancel
 *
 * Cancel AI tuning session in progress
 *
 * Returns: Success/error message
 */
bool http_rest_ai_tuning_cancel(struct fs_file *file, int num_params,
                                  char *params[], char *values[]);

/**
 * GET /rest/ai_tuning_config
 * Returns current AI tuning config (ranges, noise margin, targets)
 */
bool http_rest_ai_tuning_config_get(struct fs_file *file, int num_params,
                                       char *params[], char *values[]);

/**
 * POST /rest/ai_tuning_config_set
 * Set AI tuning config fields (any subset)
 */
bool http_rest_ai_tuning_config_set(struct fs_file *file, int num_params,
                                       char *params[], char *values[]);

#ifdef __cplusplus
}
#endif

#endif // REST_AI_TUNING_H
