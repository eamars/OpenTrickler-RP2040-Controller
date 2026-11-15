#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_
/*
 * Our own HTTP server (ADR-003, gate 2), on lwIP's sockets. One task serves up to HTTP_SERVER_SLOTS clients through
 * select(), with keep-alive, a table of routes, and replies written in pieces. Handlers run in the server's task, see
 * no lwIP and no socket, and write their reply through the calls below. The API it serves is in
 * docs/developments_plans/ADR-003-http_rest_server/api_v1.md.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// As many clients as a browser opens to one host
#define HTTP_SERVER_SLOTS 6

// A request line and the body that follows it, in one slot's buffer: an old path's query is up to 512 characters,
// a JSON body up to 1 KB, never both
#define HTTP_SERVER_REQUEST_LINE_MAX 640
#define HTTP_SERVER_BODY_MAX 1024
#define HTTP_SERVER_BUFFER_SIZE 1536

// A reply a handler writes in one go: copied into its slot when the handler returns
#define HTTP_SERVER_REPLY_MAX HTTP_SERVER_BUFFER_SIZE

typedef enum {
    HTTP_SERVER_GET,
    HTTP_SERVER_HEAD,
    HTTP_SERVER_POST,
    HTTP_SERVER_PUT,
    HTTP_SERVER_PATCH,
    HTTP_SERVER_DELETE,
    HTTP_SERVER_OPTIONS,
    HTTP_SERVER_OTHER,
} http_server_method_t;

typedef struct {
    http_server_method_t method;
    const char *path;            // decoded, with no query
    const char *query;           // still encoded, or NULL
    const char *body;            // NULL when there is none
    size_t body_len;
    bool body_is_json;           // sent as application/json
    int32_t path_id;             // the number in place of "{id}", or -1
    const char *if_none_match;   // or NULL
} http_server_request_t;

typedef struct http_server_reply http_server_reply_t;

typedef void (*http_server_handler_t)(const http_server_request_t *request, http_server_reply_t *reply);

// A reply written as it is made, chunked: fills buf with up to room bytes and gives their count in *len. Returns
// false once it has given its last bytes
typedef bool (*http_server_more_t)(void *ctx, char *buf, size_t room, size_t *len);

// A parked request's condition: true when the reply can be made
typedef bool (*http_server_ready_t)(void *ctx);

// Callbacks from the network's owner (wireless.c), so the server knows the unit's own names
typedef struct {
    bool (*host_is_own)(const char *host);           // the unit's address or host name, with or without .local
    bool (*own_address)(char *buf, size_t len);      // the unit's address, for a redirect
} http_server_network_t;

/**
 * @brief Add a route. The path may end in "/{id}", which takes a number. Routes are kept until a restart.
 * @return false when the table is full
 */
bool http_server_add_route(http_server_method_t method, const char *path, http_server_handler_t handler);

/**
 * @brief Start the server's task. Called once, when the network is up.
 */
bool http_server_start(const http_server_network_t *network);

// Replies. A handler calls exactly one of these, or parks the request.

// The shared buffer a handler writes a reply into, before it calls http_server_reply_send
char *http_server_reply_buffer(http_server_reply_t *reply, size_t *room);
// Send what was written into the buffer, with this status and type
void http_server_reply_send(http_server_reply_t *reply, int status, const char *content_type, size_t len);
// Send bytes that stay where they are until the reply is out (the pages, in flash). etag may be NULL
void http_server_reply_static(http_server_reply_t *reply, int status, const char *content_type, const char *data,
                              size_t len, const char *etag);
// Send a reply written as it is made, chunked
void http_server_reply_stream(http_server_reply_t *reply, int status, const char *content_type,
                              http_server_more_t more, void *ctx);
// An error, in api_v1.md's form: {"error": {"code": ..., "message": ...}}
void http_server_reply_error(http_server_reply_t *reply, int status, const char *code, const char *message);
// Bytes as they are, with no status line or header of ours, and the connection closed after them: an old handler's
// reply that is not HTTP (the screen's bare buffer)
void http_server_reply_raw(http_server_reply_t *reply, const char *data, size_t len);
// Hold the request until ready(ctx) is true or wait_ms has passed, then call resume to make the reply
void http_server_reply_park(http_server_reply_t *reply, http_server_ready_t ready, void *ctx, uint32_t wait_ms,
                            http_server_handler_t resume);
// While a parked request is resumed: whether it woke for its condition (true) or its time (false)
bool http_server_reply_woke_ready(const http_server_reply_t *reply);

// A small area that lives with the request until its reply is out: a streamed or a parked reply's state. NULL when
// size is more than HTTP_SERVER_CONTEXT_MAX
#define HTTP_SERVER_CONTEXT_MAX 32
void *http_server_reply_context(http_server_reply_t *reply, size_t size);

// A query's value by its name, decoded ('+' a space), into out. False when the name is not there
bool http_server_query_value(const char *query, const char *name, char *out, size_t len);

// The server's own figures, for GET /api/v1/debug
typedef struct {
    uint32_t accepted;          // connections taken
    uint32_t requests;          // requests answered
    uint32_t closed_idle;       // closed after the idle time
    uint32_t closed_for_room;   // the idlest closed to take a new client
    uint32_t aborted_stalled;   // replies that made no progress
    uint32_t refused;           // requests answered with 4xx by the server itself (not by a handler)
    uint32_t send_no_memory;    // sends that wrote nothing though select() said writable (lwIP's heap full): waited
    uint32_t send_failed;       // sends that failed otherwise: the connection closed
    uint8_t slots_used;
    uint8_t slots_parked;
} http_server_stats_t;

void http_server_get_stats(http_server_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif  // HTTP_SERVER_H_
