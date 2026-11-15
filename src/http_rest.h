#ifndef HTTP_REST_H_
#define HTTP_REST_H_
/*
 * The old /rest/... handlers' shape, until ADR-003's gate 4 deletes them. http_rest.c, the copy of lwIP's file
 * server, is gone (ADR-003, gate 2): our own server (http_server.c) serves these through an adapter
 * (rest_adapter.c), which hands a handler its query's values as http_rest.c did and sends the bytes it points at.
 */
#include <stdbool.h>

#include "lwip/apps/fs.h"

typedef bool (*rest_handler_t)(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
extern "C" {
#endif

// An old path and its handler, served on our own server by GET
void rest_register_handler(const char *uri, rest_handler_t f);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_REST_H_ */
