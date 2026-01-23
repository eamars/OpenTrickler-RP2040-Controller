#include "rest_ai_tuning.h"
#include "ai_tuning.h"
#include "profile.h"
#include "http_rest.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON response buffer
static char ai_tuning_json_buffer[2048];

// Helper function for buffer overflow errors
static bool send_buffer_overflow_error(struct fs_file *file) {
    static const char overflow_error[] = "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"success\":false,\"error\":\"Response buffer overflow\"}";
    file->data = overflow_error;
    file->len = sizeof(overflow_error) - 1;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return true;
}

bool http_rest_ai_tuning_start(struct fs_file *file, int num_params,
                                 char *params[], char *values[]) {
    int profile_idx = -1;
    int mode = -1;  // -1 = not specified, 0 = quick, 1 = fine

    // Parse parameters
    for (int idx = 0; idx < num_params; idx++) {
        if (strcmp(params[idx], "profile_idx") == 0) {
            profile_idx = atoi(values[idx]);
        }
        else if (strcmp(params[idx], "mode") == 0) {
            mode = atoi(values[idx]);
        }
    }

    // Set mode if specified
    if (mode == 0) {
        ai_tuning_set_mode(AI_TUNING_MODE_QUICK);
    } else if (mode == 1) {
        ai_tuning_set_mode(AI_TUNING_MODE_FINE);
    }

    if (profile_idx < 0 || profile_idx > 7) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Invalid profile_idx (must be 0-7)\"}",
            http_json_header);

        if (len >= 0 && len < (int)sizeof(ai_tuning_json_buffer)) {
            file->data = ai_tuning_json_buffer;
            file->len = len;
            file->index = len;
            file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        }
        return true;
    }

    // Get profile
    profile_t* profile = profile_select(profile_idx);
    if (!profile) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to select profile\"}",
            http_json_header);

        if (len >= 0 && len < (int)sizeof(ai_tuning_json_buffer)) {
            file->data = ai_tuning_json_buffer;
            file->len = len;
            file->index = len;
            file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        }
        return true;
    }

    // Start tuning
    if (!ai_tuning_start(profile)) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to start AI tuning\"}",
            http_json_header);

        if (len >= 0 && len < (int)sizeof(ai_tuning_json_buffer)) {
            file->data = ai_tuning_json_buffer;
            file->len = len;
            file->index = len;
            file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        }
        return true;
    }

    // Success
    ai_tuning_mode_t current_mode = ai_tuning_get_mode();
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"AI tuning started\",\"profile\":\"%s\",\"mode\":\"%s\",\"step\":%.2f}",
        http_json_header, profile->name,
        current_mode == AI_TUNING_MODE_QUICK ? "quick" : "fine",
        current_mode == AI_TUNING_MODE_QUICK ? 0.1f : 0.05f);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_status(struct fs_file *file, int num_params,
                                  char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    ai_tuning_session_t* session = ai_tuning_get_session();

    const char* state_str;
    switch (session->state) {
        case AI_TUNING_IDLE:           state_str = "idle"; break;
        case AI_TUNING_PHASE_1_COARSE: state_str = "phase1_coarse"; break;
        case AI_TUNING_PHASE_2_FINE:   state_str = "phase2_fine"; break;
        case AI_TUNING_COMPLETE:       state_str = "complete"; break;
        case AI_TUNING_ERROR:          state_str = "error"; break;
        default:                       state_str = "unknown"; break;
    }

    bool is_active = ai_tuning_is_active();
    bool is_complete = ai_tuning_is_complete();
    uint8_t progress = ai_tuning_get_progress_percent();
    ai_tuning_mode_t current_mode = ai_tuning_get_mode();

    // Build JSON response
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{"
        "\"state\":\"%s\","
        "\"mode\":\"%s\","
        "\"min_step\":%.2f,"
        "\"is_active\":%s,"
        "\"is_complete\":%s,"
        "\"drops_completed\":%u,"
        "\"drops_target\":%u,"
        "\"drops_max\":%u,"
        "\"progress_percent\":%u",
        http_json_header,
        state_str,
        current_mode == AI_TUNING_MODE_QUICK ? "quick" : "fine",
        current_mode == AI_TUNING_MODE_QUICK ? 0.1f : 0.05f,
        is_active ? "true" : "false",
        is_complete ? "true" : "false",
        session->drops_completed,
        session->total_drops_target,
        session->max_drops_allowed,
        progress);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    // Add current parameters if active
    if (is_active) {
        float coarse_kp, coarse_kd, fine_kp, fine_kd;
        if (ai_tuning_get_next_params(&coarse_kp, &coarse_kd, &fine_kp, &fine_kd)) {
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
                ",\"current_params\":{"
                "\"coarse_kp\":%.4f,"
                "\"coarse_kd\":%.4f,"
                "\"fine_kp\":%.4f,"
                "\"fine_kd\":%.4f"
                "}",
                coarse_kp, coarse_kd, fine_kp, fine_kd);
        }
    }

    // Add recommended parameters if complete
    if (is_complete) {
        float coarse_kp, coarse_kd, fine_kp, fine_kd;
        if (ai_tuning_get_recommended_params(&coarse_kp, &coarse_kd, &fine_kp, &fine_kd)) {
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
                ",\"recommended_params\":{"
                "\"coarse_kp\":%.4f,"
                "\"coarse_kd\":%.4f,"
                "\"fine_kp\":%.4f,"
                "\"fine_kd\":%.4f"
                "}",
                coarse_kp, coarse_kd, fine_kp, fine_kd);

            // Add statistics
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
                ",\"statistics\":{"
                "\"avg_overthrow\":%.2f,"
                "\"avg_time\":%.1f,"
                "\"consistency_score\":%.1f"
                "}",
                session->avg_overthrow,
                session->avg_total_time,
                session->consistency_score);
        }
    }

    // Add error info if in error state
    if (session->state == AI_TUNING_ERROR) {
        len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
            ",\"error_message\":\"%s\""
            ",\"best_params\":{"
            "\"coarse_kp\":%.4f,"
            "\"coarse_kd\":%.4f,"
            "\"fine_kp\":%.4f,"
            "\"fine_kd\":%.4f"
            "}",
            session->error_message,
            session->recommended_coarse_kp,
            session->recommended_coarse_kd,
            session->recommended_fine_kp,
            session->recommended_fine_kd);
    }

    // Close JSON
    len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, "}");

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_apply(struct fs_file *file, int num_params,
                                 char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    if (!ai_tuning_is_complete()) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"AI tuning not complete\"}",
            http_json_header);

        if (len >= 0 && len < (int)sizeof(ai_tuning_json_buffer)) {
            file->data = ai_tuning_json_buffer;
            file->len = len;
            file->index = len;
            file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        }
        return true;
    }

    if (!ai_tuning_apply_params()) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to apply parameters\"}",
            http_json_header);

        if (len >= 0 && len < (int)sizeof(ai_tuning_json_buffer)) {
            file->data = ai_tuning_json_buffer;
            file->len = len;
            file->index = len;
            file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        }
        return true;
    }

    // Save profile with new parameters
    profile_data_save();

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"Parameters applied and saved\"}",
        http_json_header);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_cancel(struct fs_file *file, int num_params,
                                  char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    ai_tuning_cancel();

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"AI tuning cancelled\"}",
        http_json_header);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_history(struct fs_file *file, int num_params,
                                    char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    ai_tuning_history_t* history = ai_tuning_get_history();

    // Build JSON response with drop records and suggestions
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"count\":%d,\"has_suggestions\":%s",
        http_json_header, history->count,
        history->has_suggestions ? "true" : "false");

    // Add suggestions if available
    if (history->has_suggestions) {
        len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
            ",\"suggested\":{\"coarse_kp\":%.3f,\"coarse_kd\":%.3f,\"fine_kp\":%.3f,\"fine_kd\":%.3f}",
            history->suggested_coarse_kp, history->suggested_coarse_kd,
            history->suggested_fine_kp, history->suggested_fine_kd);
    }

    // Add drop records
    len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, ",\"drops\":[");

    for (int i = 0; i < history->count && len < (int)sizeof(ai_tuning_json_buffer) - 150; i++) {
        ai_drop_record_t* d = &history->drops[i];
        if (i > 0) {
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, ",");
        }
        len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
            "{\"overthrow\":%.3f,\"time\":%.0f,\"profile\":%d}",
            d->overthrow, d->total_time_ms, d->profile_idx);
    }

    len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, "]}");

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_apply_refined(struct fs_file *file, int num_params,
                                         char *params[], char *values[]) {
    int profile_idx = -1;

    for (int idx = 0; idx < num_params; idx++) {
        if (strcmp(params[idx], "profile_idx") == 0) {
            profile_idx = atoi(values[idx]);
        }
    }

    if (profile_idx < 0 || profile_idx > 7) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Invalid profile_idx\"}",
            http_json_header);
        file->data = ai_tuning_json_buffer;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    if (!ai_tuning_apply_refined_params(profile_idx)) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"No refined values to apply\"}",
            http_json_header);
        file->data = ai_tuning_json_buffer;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"Refined values applied\"}",
        http_json_header);

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ai_tuning_clear_history(struct fs_file *file, int num_params,
                                         char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    ai_tuning_clear_history();

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"History cleared\"}",
        http_json_header);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool rest_ai_tuning_init(void) {
    // Register REST endpoints
    rest_register_handler("/rest/ai_tuning_start", http_rest_ai_tuning_start);
    rest_register_handler("/rest/ai_tuning_status", http_rest_ai_tuning_status);
    rest_register_handler("/rest/ai_tuning_apply", http_rest_ai_tuning_apply);
    rest_register_handler("/rest/ai_tuning_cancel", http_rest_ai_tuning_cancel);
    rest_register_handler("/rest/ai_tuning_history", http_rest_ai_tuning_history);
    rest_register_handler("/rest/ai_tuning_apply_refined", http_rest_ai_tuning_apply_refined);
    rest_register_handler("/rest/ai_tuning_clear_history", http_rest_ai_tuning_clear_history);

    printf("AI Tuning REST endpoints registered:\n");
    printf("  - POST /rest/ai_tuning_start?profile_idx=X\n");
    printf("  - GET  /rest/ai_tuning_status\n");
    printf("  - POST /rest/ai_tuning_apply\n");
    printf("  - POST /rest/ai_tuning_cancel\n");
    printf("  - GET  /rest/ai_tuning_history\n");
    printf("  - POST /rest/ai_tuning_apply_refined?profile_idx=X\n");
    printf("  - POST /rest/ai_tuning_clear_history\n");

    return true;
}
