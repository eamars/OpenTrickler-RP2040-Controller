#ifndef REST_AI_TUNING_H
#define REST_AI_TUNING_H

#include <stdbool.h>
#include "lwip/apps/fs.h"

/**
 * REST API Endpoints for AI PID Auto-Tuning
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
 * Start AI tuning session for specified profile
 *
 * Parameters:
 * - profile_idx: Profile index (0-7)
 *
 * Returns: Success/error message
 */
bool http_rest_ai_tuning_start(struct fs_file *file, int num_params,
                                 char *params[], char *values[]);

/**
 * GET /rest/ai_tuning_status
 *
 * Returns JSON with:
 * - state: "idle", "phase1_coarse", "phase2_fine", "complete", "error"
 * - drops_completed: Number of drops completed
 * - drops_target: Target number of drops
 * - progress_percent: Progress percentage (0-100)
 * - current_params: Current Kp/Kd being tested
 * - recommended_params: Recommended values (if complete)
 * - statistics: Performance statistics
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

#ifdef __cplusplus
}
#endif

#endif // REST_AI_TUNING_H
