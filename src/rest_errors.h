#ifndef REST_ERRORS_H_
#define REST_ERRORS_H_

#include <lwip/apps/fs.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// REST endpoint handler for error log
// Returns JSON: {"count":N,"errors":[{"code":105,"cat":"EEP","msg":"EEPROM alloc"},...]}
bool http_rest_errors(struct fs_file *file, int num_params, char *params[], char *values[]);

// REST endpoint to clear all errors
// Returns JSON: {"success":true,"message":"Errors cleared"}
bool http_rest_clear_errors(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif

#endif // REST_ERRORS_H_
