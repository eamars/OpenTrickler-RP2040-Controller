#include "rest_ai_tuning.h"
#include "ai_tuning.h"
#include "profile.h"
#include "http_rest.h"
#include "common.h"
#include "app.h"
#include "app_state.h"
#include "encoder.h"
#include "charge_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <queue.h>

// External references for triggering charge mode
extern charge_mode_config_t charge_mode_config;

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
    float target_weight = -1.0f;  // -1 = not specified, use default

    // Parse parameters
    for (int idx = 0; idx < num_params; idx++) {
        if (strcmp(params[idx], "profile_idx") == 0) {
            profile_idx = atoi(values[idx]);
        }
        else if (strcmp(params[idx], "target") == 0) {
            target_weight = strtof(values[idx], NULL);
        }
    }

    // Set target weight - use provided value or default to 30.0 grains
    if (target_weight <= 0.0f) {
        target_weight = 30.0f;  // Reasonable default for powder charge
    }
    charge_mode_config.target_charge_weight = target_weight;
    printf("AI Tuning: Target weight set to %.2f grains\n", target_weight);

    if (profile_idx < 0 || profile_idx > 7) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Invalid profile_idx (must be 0-7)\"}",
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

    // Get profile
    profile_t* profile = profile_select(profile_idx);
    if (!profile) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to select profile\"}",
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

    // Start tuning
    if (!ai_tuning_start(profile)) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to start AI tuning\"}",
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

    // CRITICAL FIX: Trigger entering charge mode so tuning actually runs
    // Without this, ai_tuning just sets internal state but nothing happens
    if (charge_mode_config.charge_mode_state == CHARGE_MODE_EXIT) {
        // Set exit_state to enter charge mode from REST
        exit_state = APP_STATE_ENTER_CHARGE_MODE_FROM_REST;

        // Signal the menu to transition to charge mode
        ButtonEncoderEvent_t button_event = OVERRIDE_FROM_REST;
        xQueueSend(encoder_event_queue, &button_event, 0);

        printf("AI Tuning: Triggering charge mode entry\n");
    }

    // Success
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"AI tuning started - entering charge mode\","
        "\"profile\":\"%s\",\"target_weight\":%.2f}",
        http_json_header, profile->name, target_weight);

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

    ai_tuning_session_t session_copy;
    ai_tuning_get_session_copy(&session_copy);
    ai_tuning_session_t* session = &session_copy;

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

    // Build JSON response
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{"
        "\"state\":\"%s\","
        "\"is_active\":%s,"
        "\"is_complete\":%s,"
        "\"drops_completed\":%u,"
        "\"drops_max\":%u,"
        "\"progress_percent\":%u",
        http_json_header,
        state_str,
        is_active ? "true" : "false",
        is_complete ? "true" : "false",
        session->drops_completed,
        session->max_drops_allowed,
        progress);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    // Add current parameters if active
    if (is_active && len < (int)sizeof(ai_tuning_json_buffer)) {
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
            if (len >= (int)sizeof(ai_tuning_json_buffer)) {
                return send_buffer_overflow_error(file);
            }
        }

        // Add last drop result if we have any drops
        if (session->drops_completed > 0) {
            uint8_t last_idx = (session->drop_write_idx + AI_TUNING_DROP_BUF_SIZE - 1) % AI_TUNING_DROP_BUF_SIZE;
            ai_drop_telemetry_t* last = &session->drops[last_idx];
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
                ",\"last_drop\":{"
                "\"drop_num\":%u,"
                "\"overthrow\":%.4f,"
                "\"coarse_time_ms\":%.0f,"
                "\"fine_time_ms\":%.0f,"
                "\"total_time_ms\":%.0f,"
                "\"kp\":%.4f,"
                "\"kd\":%.4f"
                "}",
                last->drop_number,
                last->overthrow,
                last->coarse_time_ms,
                last->fine_time_ms,
                last->total_time_ms,
                (session->state == AI_TUNING_PHASE_1_COARSE) ? last->coarse_kp_used : last->fine_kp_used,
                (session->state == AI_TUNING_PHASE_1_COARSE) ? last->coarse_kd_used : last->fine_kd_used);
            if (len >= (int)sizeof(ai_tuning_json_buffer)) {
                return send_buffer_overflow_error(file);
            }
        }
    }

    // Add recommended parameters if complete
    if (is_complete && len < (int)sizeof(ai_tuning_json_buffer)) {
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
            if (len >= (int)sizeof(ai_tuning_json_buffer)) {
                return send_buffer_overflow_error(file);
            }

            // Add statistics
            len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
                ",\"statistics\":{"
                "\"avg_overthrow\":%.2f,"
                "\"avg_time\":%.1f"
                "}",
                session->avg_overthrow,
                session->avg_total_time);
            if (len >= (int)sizeof(ai_tuning_json_buffer)) {
                return send_buffer_overflow_error(file);
            }
        }
    }

    // Add error info if in error state
    if (session->state == AI_TUNING_ERROR && len < (int)sizeof(ai_tuning_json_buffer)) {
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
        if (len >= (int)sizeof(ai_tuning_json_buffer)) {
            return send_buffer_overflow_error(file);
        }
    }

    // Close JSON
    if (len < (int)sizeof(ai_tuning_json_buffer)) {
        len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, "}");
    }

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

        if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
            return send_buffer_overflow_error(file);
        }
        file->data = ai_tuning_json_buffer;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    if (!ai_tuning_apply_params()) {
        int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
            "%s{\"success\":false,\"error\":\"Failed to apply parameters\"}",
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

    ai_tuning_history_t history_copy;
    ai_tuning_get_history_copy(&history_copy);
    ai_tuning_history_t* history = &history_copy;

    // Build JSON response with drop records and suggestions
    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"count\":%d,\"has_suggestions\":%s",
        http_json_header, history->count,
        history->has_suggestions ? "true" : "false");

    // Add suggestions if available (ML only suggests fine params)
    if (history->has_suggestions) {
        len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len,
            ",\"suggested\":{\"fine_kp\":%.3f,\"fine_kd\":%.3f}",
            history->suggested_fine_kp, history->suggested_fine_kd);
        if (len >= (int)sizeof(ai_tuning_json_buffer)) {
            return send_buffer_overflow_error(file);
        }
    }

    // Add drop records
    len += snprintf(ai_tuning_json_buffer + len, sizeof(ai_tuning_json_buffer) - len, ",\"drops\":[");

    for (int i = 0; i < history->count && i < AI_TUNING_HISTORY_SIZE && len < (int)sizeof(ai_tuning_json_buffer) - 150; i++) {
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
        if (len < 0) len = 0;
        if (len >= (int)sizeof(ai_tuning_json_buffer)) len = (int)sizeof(ai_tuning_json_buffer) - 1;
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
        if (len < 0) len = 0;
        if (len >= (int)sizeof(ai_tuning_json_buffer)) len = (int)sizeof(ai_tuning_json_buffer) - 1;
        file->data = ai_tuning_json_buffer;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"Refined values applied\"}",
        http_json_header);
    if (len < 0) len = 0;
    if (len >= (int)sizeof(ai_tuning_json_buffer)) len = (int)sizeof(ai_tuning_json_buffer) - 1;

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


bool http_rest_ai_tuning_config_get(struct fs_file *file, int num_params,
                                       char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    ai_tuning_config_t* cfg = ai_tuning_get_config();

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{"
        "\"coarse_kp_min\":%.3f,\"coarse_kp_max\":%.3f,"
        "\"coarse_kd_min\":%.3f,\"coarse_kd_max\":%.3f,"
        "\"fine_kp_min\":%.3f,\"fine_kp_max\":%.3f,"
        "\"fine_kd_min\":%.3f,\"fine_kd_max\":%.3f,"
        "\"noise_margin\":%.3f,"
        "\"max_overthrow_percent\":%.2f"
        "}",
        http_json_header,
        cfg->coarse_kp_min, cfg->coarse_kp_max,
        cfg->coarse_kd_min, cfg->coarse_kd_max,
        cfg->fine_kp_min, cfg->fine_kp_max,
        cfg->fine_kd_min, cfg->fine_kd_max,
        cfg->noise_margin,
        cfg->max_overthrow_percent);

    if (len < 0 || len >= (int)sizeof(ai_tuning_json_buffer)) {
        return send_buffer_overflow_error(file);
    }

    file->data = ai_tuning_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return true;
}

bool http_rest_ai_tuning_config_set(struct fs_file *file, int num_params,
                                       char *params[], char *values[]) {
    ai_tuning_config_t* cfg = ai_tuning_get_config();

    for (int idx = 0; idx < num_params; idx++) {
        if (strcmp(params[idx], "coarse_kp_min") == 0)
            cfg->coarse_kp_min = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "coarse_kp_max") == 0)
            cfg->coarse_kp_max = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "coarse_kd_min") == 0)
            cfg->coarse_kd_min = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "coarse_kd_max") == 0)
            cfg->coarse_kd_max = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "fine_kp_min") == 0)
            cfg->fine_kp_min = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "fine_kp_max") == 0)
            cfg->fine_kp_max = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "fine_kd_min") == 0)
            cfg->fine_kd_min = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "fine_kd_max") == 0)
            cfg->fine_kd_max = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "noise_margin") == 0)
            cfg->noise_margin = strtof(values[idx], NULL);
        else if (strcmp(params[idx], "max_overthrow_percent") == 0)
            cfg->max_overthrow_percent = strtof(values[idx], NULL);
    }

    printf("AI Tuning config updated: C_Kp[%.3f-%.3f] C_Kd[%.3f-%.3f] F_Kp[%.3f-%.3f] F_Kd[%.3f-%.3f] noise=%.3f\n",
           cfg->coarse_kp_min, cfg->coarse_kp_max,
           cfg->coarse_kd_min, cfg->coarse_kd_max,
           cfg->fine_kp_min, cfg->fine_kp_max,
           cfg->fine_kd_min, cfg->fine_kd_max,
           cfg->noise_margin);

    // Config applied to memory - user must click Save to persist to EEPROM
    // (follows same pattern as motor config, scale config, etc.)

    int len = snprintf(ai_tuning_json_buffer, sizeof(ai_tuning_json_buffer),
        "%s{\"success\":true,\"message\":\"Config applied (click Save to persist)\"}",
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
    rest_register_handler("/rest/ai_tuning_config", http_rest_ai_tuning_config_get);
    rest_register_handler("/rest/ai_tuning_config_set", http_rest_ai_tuning_config_set);

    printf("AI Tuning REST endpoints registered\n");

    return true;
}
