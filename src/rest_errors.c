#include <stdio.h>
#include <string.h>

#include "rest_errors.h"
#include "error.h"
#include "common.h"

// Get short category string for JSON output
static const char* error_get_short_cat(error_code_t code) {
    if (code >= 100 && code < 200) return "EEP";
    if (code >= 200 && code < 300) return "DSP";
    if (code >= 300 && code < 400) return "LED";
    if (code >= 400 && code < 500) return "WIF";
    if (code >= 500 && code < 600) return "MOT";
    if (code >= 600 && code < 700) return "SCL";
    if (code >= 700 && code < 800) return "SRV";
    if (code >= 800 && code < 900) return "PRF";
    if (code >= 900 && code < 1000) return "CHG";
    if (code >= 1000 && code < 1100) return "RST";
    if (code >= 1100 && code < 1200) return "MEM";
    if (code >= 1200 && code < 1300) return "CAL";
    return "UNK";
}

bool http_rest_errors(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Buffer for JSON response
    // Max size: header (~60) + {"count":N,"errors":[ (~25) + 8 errors * ~60 chars each + ]}
    static char errors_json_buffer[600];

    // Check for clear parameter
    for (int idx = 0; idx < num_params; idx++) {
        if (strcmp(params[idx], "clear") == 0) {
            if (strcmp(values[idx], "1") == 0 || strcmp(values[idx], "true") == 0) {
                error_clear_last();
            }
        }
    }

    uint8_t count = error_get_count();

    // Start building JSON
    int offset = snprintf(errors_json_buffer, sizeof(errors_json_buffer),
                          "%s{\"count\":%d,\"errors\":[",
                          http_json_header, count);

    // Add each error
    for (uint8_t i = 0; i < count && offset < (int)sizeof(errors_json_buffer) - 80; i++) {
        error_code_t code = error_get_at(i);

        if (i > 0) {
            offset += snprintf(errors_json_buffer + offset,
                               sizeof(errors_json_buffer) - offset, ",");
        }

        offset += snprintf(errors_json_buffer + offset,
                           sizeof(errors_json_buffer) - offset,
                           "{\"code\":%d,\"cat\":\"%s\",\"msg\":\"%s\"}",
                           (int)code,
                           error_get_short_cat(code),
                           error_code_to_string(code));
    }

    // Close JSON
    snprintf(errors_json_buffer + offset, sizeof(errors_json_buffer) - offset, "]}");

    size_t data_length = strlen(errors_json_buffer);
    file->data = errors_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_clear_errors(struct fs_file *file, int num_params, char *params[], char *values[]) {
    (void)num_params;
    (void)params;
    (void)values;

    static char clear_json_buffer[128];

    // Clear all errors
    while (error_get_count() > 0) {
        error_clear_last();
    }

    int len = snprintf(clear_json_buffer, sizeof(clear_json_buffer),
                       "%s{\"success\":true,\"message\":\"Errors cleared\"}",
                       http_json_header);

    file->data = clear_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
