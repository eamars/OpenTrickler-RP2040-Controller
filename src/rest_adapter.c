/*
 * The old /rest/... paths on our own server, until ADR-003's gate 4 deletes them with their handlers. A handler is
 * given its query's values as http_rest.c gave them, and its reply is copied at once, so the next request to the
 * same path cannot rewrite it while it is sent. A reply that starts with an HTTP status line is sent again with our
 * own headers, so it keeps its connection open; any other is sent as it is, and the connection closed after it.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_rest.h"
#include "http_server.h"


// As http_rest.c took them (ADR-012; audit H1 and H2): up to 20 values, a query of up to 511 characters
#define REST_ADAPTER_PATHS 24
#define REST_ADAPTER_VALUES 20
#define REST_ADAPTER_QUERY_MAX 512


typedef struct {
    const char *uri;
    rest_handler_t handler;
} rest_adapter_path_t;

static rest_adapter_path_t rest_adapter_paths[REST_ADAPTER_PATHS];
static int rest_adapter_path_count = 0;

// Static, as in http_rest.c: the handlers run one at a time, in the server's task
static char rest_adapter_query[REST_ADAPTER_QUERY_MAX];
static char *rest_adapter_params[REST_ADAPTER_VALUES];
static char *rest_adapter_values[REST_ADAPTER_VALUES];
static char rest_adapter_empty[] = "";


// %xx and '+' decoded in place
static void rest_adapter_decode(char *s) {
    char *dst = s;
    for (const char *src = s; *src; ) {
        if (src[0] == '%' && isxdigit((unsigned char) src[1]) && isxdigit((unsigned char) src[2])) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        }
        else if (*src == '+') {
            *dst++ = ' ';
            src += 1;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}


// Split the query on '&' then '=' before decoding, so a value may hold an encoded '&' (PR #112)
static int rest_adapter_split(const char *query) {
    if (query == NULL || query[0] == '\0') {
        return 0;
    }
    if (strlen(query) >= sizeof(rest_adapter_query)) {
        // Longer still: refused whole, as http_rest.c did. A form cut short would change settings to values nobody sent
        printf("HTTP: a query of %u characters is refused\n", (unsigned) strlen(query));
        return 0;
    }
    strcpy(rest_adapter_query, query);
    int count = 0;
    char *pair = rest_adapter_query;
    while (pair != NULL && *pair && count < REST_ADAPTER_VALUES) {
        char *next = strchr(pair, '&');
        if (next != NULL) {
            *next = '\0';
            next += 1;
        }
        char *equals = strchr(pair, '=');
        rest_adapter_params[count] = pair;
        if (equals != NULL) {
            *equals = '\0';
            rest_adapter_values[count] = equals + 1;
            rest_adapter_decode(rest_adapter_values[count]);
        }
        else {
            // A name with no '=': http_rest.c gave the handler NULL, which every handler used as a string. Now ""
            rest_adapter_values[count] = rest_adapter_empty;
        }
        count += 1;
        pair = next;
    }
    return count;
}


// Where a reply's own headers end, within its length (the data need not end in a NUL)
static const char *rest_adapter_body(const char *data, size_t len) {
    for (size_t idx = 0; idx + 3 < len; idx += 1) {
        if (data[idx] == '\r' && data[idx + 1] == '\n' && data[idx + 2] == '\r' && data[idx + 3] == '\n') {
            return data + idx + 4;
        }
    }
    return NULL;
}


static void rest_adapter_handle(const http_server_request_t *request, http_server_reply_t *reply) {
    rest_handler_t handler = NULL;
    for (int idx = 0; idx < rest_adapter_path_count; idx += 1) {
        if (strcmp(rest_adapter_paths[idx].uri, request->path) == 0) {
            handler = rest_adapter_paths[idx].handler;
            break;
        }
    }
    if (handler == NULL) {
        http_server_reply_error(reply, 404, "not_found", "no such path on this build");
        return;
    }

    int count = rest_adapter_split(request->query);
    struct fs_file file;
    memset(&file, 0, sizeof(file));
    handler(&file, count, rest_adapter_params, rest_adapter_values);

    const char *data = file.data;
    size_t len = file.len > 0 ? (size_t) file.len : 0;
    if (data == NULL || len == 0) {
        http_server_reply_error(reply, 500, "failed", "the old handler gave no reply");
        return;
    }
    const char *body = len > 5 && strncmp(data, "HTTP/", 5) == 0 ? rest_adapter_body(data, len) : NULL;
    if (body == NULL) {
        // Not HTTP (the screen's bare buffer, a reply with no header): as it is, and the connection closed after
        http_server_reply_raw(reply, data, len);
        return;
    }

    // The old reply's status, sent again with our own headers. Every old reply with a status line is JSON
    // (http_json_header, common.c)
    int status = (int) strtol(data + 9, NULL, 10);
    if (status < 100 || status > 599) {
        status = 200;
    }
    const char *content_type = "application/json";
    size_t body_len = len - (size_t) (body - data);
    size_t room;
    char *out = http_server_reply_buffer(reply, &room);
    if (body_len > room) {
        body_len = room;
    }
    memcpy(out, body, body_len);
    http_server_reply_send(reply, status, content_type, body_len);
}


void rest_register_handler(const char *uri, rest_handler_t f) {
    if (rest_adapter_path_count >= REST_ADAPTER_PATHS) {
        printf("HTTP: no room for the old path %s\n", uri);
        return;
    }
    rest_adapter_paths[rest_adapter_path_count].uri = uri;
    rest_adapter_paths[rest_adapter_path_count].handler = f;
    rest_adapter_path_count += 1;
    http_server_add_route(HTTP_SERVER_GET, uri, rest_adapter_handle);
}
