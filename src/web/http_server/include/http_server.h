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

// A request line and the body behind it share one slot's buffer: a line up to 640 characters, a JSON body up to 1 KB,
// not both at their longest. A longer body goes to an upload route in pieces (http_server_add_upload)
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
    const char *last_event_id;   // Last-Event-ID, which a browser's event source sends when it reconnects; or NULL
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
    void (*listening)(void);                         // once, when the server first listens; may be NULL
} http_server_network_t;

/**
 * @brief Add a route. The path may end in "/{id}", which takes a number. Routes are kept until a restart.
 * @return false when the table is full
 */
bool http_server_add_route(http_server_method_t method, const char *path, http_server_handler_t handler);

// A route that takes its body in pieces as it comes, never held whole: ADR-004's image, a PUT of bytes of any length.
// The handler keeps its state in the request's context (http_server_reply_context in begin and end; ctx in piece and
// abort). Once begin has taken a body, exactly one of end and abort is called
typedef struct {
    // The headers are in, none of the body: take it (true), or refuse it with a reply made (false), and the body is
    // read and dropped. request->body_len is the body's length; request->body is NULL throughout
    bool (*begin)(const http_server_request_t *request, http_server_reply_t *reply);
    // The next piece of the body, in order. False stops the upload: end is called at once, and the rest of the body
    // is read and dropped
    bool (*piece)(void *ctx, const char *data, size_t len);
    // The body is all in, or a piece stopped it: make the reply
    void (*end)(const http_server_request_t *request, http_server_reply_t *reply);
    // The connection went before the body was all in, or stalled: no reply goes
    void (*abort)(void *ctx);
} http_server_upload_t;

/**
 * @brief Add a route whose body comes to its handler in pieces (see http_server_upload_t). upload is kept, not copied
 * @return false when the table is full
 */
bool http_server_add_upload(http_server_method_t method, const char *path, const http_server_upload_t *upload);

/**
 * @brief Start the server's task. Called once, when the network is up.
 */
bool http_server_start(const http_server_network_t *network);

/**
 * @brief Whether a write to the API (a POST, PUT, PATCH or DELETE under /api/) was admitted in the last ms: the screen's
 *        remote mark (ADR-010, gate 4). Any task.
 */
bool http_server_wrote_within(uint32_t ms);

// Replies. A handler calls exactly one of these, or parks the request.

// The shared buffer a handler writes a reply into, before it calls http_server_reply_send
char *http_server_reply_buffer(http_server_reply_t *reply, size_t *room);
// Send what was written into the buffer, with this status and type
void http_server_reply_send(http_server_reply_t *reply, int status, const char *content_type, size_t len);
// Send bytes that stay where they are until the reply is out (the pages, in flash). etag may be NULL
void http_server_reply_static(http_server_reply_t *reply, int status, const char *content_type, const char *data,
                              size_t len, const char *etag);
// The same, its bytes gzipped: sent with Content-Encoding: gzip (ADR-008, gate 3: the portal)
void http_server_reply_static_gzip(http_server_reply_t *reply, int status, const char *content_type, const char *data,
                                   size_t len, const char *etag);
// A redirect (302) to a location of the unit's own ("/#setup"), up to 47 characters, nothing in its body
void http_server_reply_redirect(http_server_reply_t *reply, const char *location);
// Send a reply written as it is made, chunked
void http_server_reply_stream(http_server_reply_t *reply, int status, const char *content_type,
                              http_server_more_t more, void *ctx);
// A reply that stays open, written as things happen (ADR-008, gate 2: the event stream). Chunked, made by `more` as
// a stream's, except that `more` may give nothing for now (true, *len 0): the slot then waits, sending nothing and
// never counted as stalled, until ready(ctx) says there is more (looked at each tick, 50 ms). Bytes the client leaves
// unread are the stall limit's, as any reply's: a client that stops reading is closed. At most HTTP_SERVER_LIVE_MAX
// at once, so a request always finds a slot: one past them is answered 503 with Retry-After
#define HTTP_SERVER_LIVE_MAX 3
void http_server_reply_live(http_server_reply_t *reply, const char *content_type, http_server_more_t more,
                            http_server_ready_t ready, void *ctx);
// Give a stream a tag (the schema, the build's hash): answered 304 with nothing made when the client has it
void http_server_reply_etag(http_server_reply_t *reply, const char *etag);
// An error, in api_v1.md's form: {"error": {"code": ..., "message": ...}}
void http_server_reply_error(http_server_reply_t *reply, int status, const char *code, const char *message);
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

// A GET of a path that takes only other methods (an action): answered by this handler with the action's usage
// (ADR-008, gate 2). A handler that gives no reply leaves it 405, as before
void http_server_set_usage(http_server_handler_t usage);

/*
 * Every write to /api/ (a POST, PUT, PATCH or DELETE) asks the guard first, with the request's Cookie `pin=` value
 * ("" with none, or a Cookie line too long to keep) (ADR-016, gate 3). It answers 0 to let it through, or the status
 * to refuse it with, its code and message, and a Retry-After in seconds or 0. With no guard handed over, every write
 * goes through
 */
typedef int (*http_server_write_guard_t)(const char *pin, const char **code, const char **message,
                                         uint32_t *retry_after_s);
void http_server_set_write_guard(http_server_write_guard_t guard);

// The routes, one at a time, for the check that every route is in the index (ADR-008, gate 2). False past the last
bool http_server_route_at(int idx, http_server_method_t *method, const char **path);
const char *http_server_method_name(http_server_method_t method);

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
    uint32_t stream_make_ms;    // time in the handlers' `more`, making streamed chunks, the task's preemption included
    uint32_t stream_bytes;      // the bytes they made
    uint32_t stream_failed;     // streams cut off: a chunk that overflowed, or no progress (a fault of the build)
    uint32_t live_opened;       // replies that stay open (the event stream), opened
    uint32_t live_refused;      // and refused, past HTTP_SERVER_LIVE_MAX
    uint8_t slots_used;
    uint8_t slots_parked;
    uint8_t slots_live;         // open now: waiting, or sending what came
} http_server_stats_t;

void http_server_get_stats(http_server_stats_t *stats);

/**
 * @brief Each connection taken and each request on the console, with the client's address and the unit's time, or
 * not (the default). Never stored: a link's rhythm read from a client's own reads
 */
void http_server_log_requests(bool on);

/**
 * @brief Before a restart the unit means: what the server holds is sent, then every connection is closed and the
 * listener with it, so each client learns at once that the unit has gone, not by a request that hangs. Waits up to
 * wait_ms for the server's task to do it; called from any task but the server's. Nothing is accepted after it
 */
void http_server_close_all(uint32_t wait_ms);

// The server's command handed to the remote control's console (http_server_console.c): net. From the start-up's
// wiring, before the console's task runs. False when the console has no room for it
bool http_server_console_init(void);

#ifdef __cplusplus
}
#endif

#endif  // HTTP_SERVER_H_
