/*
 * Our own HTTP server (ADR-003, gate 2): one task, lwIP's sockets, select() over the listener and up to
 * HTTP_SERVER_SLOTS clients. A request is read a line at a time; the headers the server uses are kept and the rest
 * passed over. A body is held whole up to 1 KB, or handed to an upload's route in pieces as it comes (gate 4). A reply
 * is a status and headers of ours, then a body from the slot's buffer, from flash, or written as it is made
 * (chunked). Keep-alive, an idle limit, a stall limit, and requests that can be parked.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/time.h"

#include "lwip/sockets.h"
#include "lwip/errno.h"

#include "http_server.h"

// lwIP is built with an errno of its own (LWIP_PROVIDE_ERRNO, set by the SDK's cyw43_arch_lwip): its sockets set the
// int `errno` lwIP declares, not newlib's, which is a macro over __errno(). Read with newlib's, a send that would
// block read as errno 0, and every page longer than the send buffer was cut off (ADR-003, gate 2)
#if defined(errno)
#error "newlib's errno is in scope here: lwIP's sockets set their own"
#endif


#define HTTP_SERVER_PORT 80

// Below the menu's task (6), which runs the charge loop, and the charge screen's (5): the server formats floats on a
// chip with no FPU and must never take the loop's turn. Below the cyw43 task too (4, the SDK's), which takes in the
// network's packets and the ACKs the server's replies wait for. lwIP's own thread is at 7
#define HTTP_SERVER_TASK_PRIORITY 3
#define HTTP_SERVER_TASK_STACK_WORDS 1536

#define HTTP_SERVER_ROUTES 80                // gate 3's settings add some 30 to the 32 before them
#define HTTP_SERVER_LINE_MAX 256            // a header line the server reads; longer ones are passed over
#define HTTP_SERVER_HEADERS_MAX 8192        // all of a request's headers, read or passed over
#define HTTP_SERVER_IDLE_MS 5000            // a kept-alive connection with no request in this long is closed
#define HTTP_SERVER_STALL_MS 10000          // a request or a reply that makes no progress this long is aborted
#define HTTP_SERVER_PARK_MAX 2
#define HTTP_SERVER_TICK_MS 50              // select() wakes at least this often, for the timers and parked slots
#define HTTP_SERVER_NO_MEMORY_WAIT_MS 10    // a send lwIP had no memory for waits this long
#define HTTP_SERVER_DRAIN_MS 2000           // the longest a refused request's unread rest is read before the close
#define HTTP_SERVER_CHUNK_HEAD 8            // room before a chunk for its length: "ffff\r\n"


typedef enum {
    SLOT_FREE = 0,
    SLOT_REQUEST,       // reading a request, or waiting for the next one on a kept-alive connection
    SLOT_PARKED,
    SLOT_SENDING,
    SLOT_DRAINING,      // the reply is out, the request was not all read: what is left is read and dropped
} slot_state_t;

typedef enum {
    PHASE_LINE = 0,
    PHASE_HEADERS,
    PHASE_BODY,
    PHASE_UPLOAD,       // a body handed to its upload route in pieces
} slot_phase_t;

typedef enum {
    BODY_NONE = 0,
    BODY_BUFFER,        // in the slot's buffer
    BODY_STATIC,        // where it lies, in flash
    BODY_STREAM,        // written as it is made, a chunk at a time, into the slot's buffer
} body_kind_t;

typedef struct {
    int fd;
    slot_state_t state;
    slot_phase_t phase;
    uint32_t last_progress_ms;
    uint32_t no_memory_until_ms;

    // The request
    char buf[HTTP_SERVER_BUFFER_SIZE];     // the request line, then the body
    size_t buf_len;
    size_t line_end;                       // where the request line ends (its NUL)
    char line[HTTP_SERVER_LINE_MAX];       // a header line; then the reply's status line and headers
    size_t line_len;
    bool line_skip;                        // passing over a header line too long to keep
    size_t headers_total;
    bool started;                          // a byte of this request has come
    bool http10;
    bool keep_alive;
    bool close_after;
    bool drain;                            // input may be left unread: close by reading it first
    uint32_t drain_until_ms;
    bool has_host;
    char host[64];
    char content_type[48];
    char if_none_match[48];
    long content_length;
    bool chunked_request;
    bool expect_continue;
    bool connection_close;
    bool connection_keep_alive;
    http_server_request_t request;

    // A body taken in pieces
    const http_server_upload_t *upload;
    size_t upload_left;
    bool upload_open;                      // begin took it; neither end nor abort has been called

    // The reply
    size_t head_len;
    size_t head_sent;
    body_kind_t body_kind;
    const char *body_ptr;
    size_t body_len;
    size_t body_sent;
    bool body_suppressed;                  // HEAD, or 304
    bool chunked;
    http_server_more_t more;
    void *more_ctx;
    bool more_done;
    bool last_chunk_sent;

    // Parked
    http_server_ready_t park_ready;
    void *park_ctx;
    uint32_t park_deadline_ms;
    http_server_handler_t park_resume;

    // A handler's state for a streamed or parked reply
    uint32_t context[HTTP_SERVER_CONTEXT_MAX / sizeof(uint32_t)];
} slot_t;

struct http_server_reply {
    slot_t *slot;
    bool done;
    bool parked;
    bool woke_ready;
    int status;
    const char *content_type;
    size_t len;                 // of the shared buffer's reply
    body_kind_t kind;
    const char *data;
    size_t data_len;
    const char *etag;
    bool raw;
    http_server_more_t more;
    void *more_ctx;
    char location[48];
    char allow[48];
};

typedef struct {
    http_server_method_t method;
    const char *path;           // as given, with "{id}" where a number goes
    size_t prefix_len;          // before "{id}", or the whole path
    const char *suffix;         // after "{id}", or NULL
    http_server_handler_t handler;
    const http_server_upload_t *upload;     // a body in pieces, in place of handler; or NULL
} route_t;


static route_t routes[HTTP_SERVER_ROUTES];
static int route_count = 0;
static slot_t *slots = NULL;
static char reply_scratch[HTTP_SERVER_REPLY_MAX];
static http_server_network_t network;
static http_server_stats_t stats;
static uint64_t stream_make_us;     // stats.stream_make_ms, kept in microseconds: a chunk takes some milliseconds
static TaskHandle_t server_task_handle = NULL;


static uint32_t now_ms(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


static const char *status_text(int status) {
    switch (status) {
        case 100: return "Continue";
        case 200: return "OK";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 413: return "Content Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Unknown";
    }
}


static const char *method_name(http_server_method_t method) {
    switch (method) {
        case HTTP_SERVER_GET: return "GET";
        case HTTP_SERVER_HEAD: return "HEAD";
        case HTTP_SERVER_POST: return "POST";
        case HTTP_SERVER_PUT: return "PUT";
        case HTTP_SERVER_PATCH: return "PATCH";
        case HTTP_SERVER_DELETE: return "DELETE";
        case HTTP_SERVER_OPTIONS: return "OPTIONS";
        default: return "?";
    }
}


static http_server_method_t parse_method(const char *s, size_t len) {
    static const struct { const char *name; http_server_method_t method; } methods[] = {
        {"GET", HTTP_SERVER_GET}, {"HEAD", HTTP_SERVER_HEAD}, {"POST", HTTP_SERVER_POST}, {"PUT", HTTP_SERVER_PUT},
        {"PATCH", HTTP_SERVER_PATCH}, {"DELETE", HTTP_SERVER_DELETE}, {"OPTIONS", HTTP_SERVER_OPTIONS},
    };
    for (size_t idx = 0; idx < sizeof(methods) / sizeof(methods[0]); idx += 1) {
        if (strlen(methods[idx].name) == len && memcmp(methods[idx].name, s, len) == 0) {
            return methods[idx].method;
        }
    }
    return HTTP_SERVER_OTHER;
}


bool http_server_add_route(http_server_method_t method, const char *path, http_server_handler_t handler) {
    if (route_count >= HTTP_SERVER_ROUTES) {
        printf("HTTP: no room for the route %s %s\n", method_name(method), path);
        return false;
    }
    route_t *route = &routes[route_count];
    route->method = method;
    route->path = path;
    route->handler = handler;
    route->upload = NULL;
    const char *id = strstr(path, "{id}");
    if (id != NULL) {
        route->prefix_len = (size_t) (id - path);
        route->suffix = id + 4;
    }
    else {
        route->prefix_len = strlen(path);
        route->suffix = NULL;
    }
    route_count += 1;
    return true;
}


bool http_server_add_upload(http_server_method_t method, const char *path, const http_server_upload_t *upload) {
    if (!http_server_add_route(method, path, NULL)) {
        return false;
    }
    routes[route_count - 1].upload = upload;
    return true;
}


// Whether a route's path takes this path; the number in place of "{id}" goes to *id
static bool route_matches(const route_t *route, const char *path, int32_t *id) {
    if (route->suffix == NULL) {
        *id = -1;
        return strcmp(route->path, path) == 0;
    }
    if (strncmp(route->path, path, route->prefix_len) != 0) {
        return false;
    }
    const char *digits = path + route->prefix_len;
    const char *end = digits;
    while (isdigit((unsigned char) *end) && end - digits < 9) {
        end += 1;
    }
    if (end == digits || strcmp(end, route->suffix) != 0) {
        return false;
    }
    *id = (int32_t) strtol(digits, NULL, 10);
    return true;
}


// Decode %xx in a path, in place. A path's '+' stays a '+'
static void decode_path(char *s) {
    char *dst = s;
    for (const char *src = s; *src; ) {
        if (src[0] == '%' && isxdigit((unsigned char) src[1]) && isxdigit((unsigned char) src[2])) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}


static bool header_is(const char *line, size_t name_len, const char *name) {
    return strlen(name) == name_len && strncasecmp(line, name, name_len) == 0;
}


static const char *skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s += 1;
    }
    return s;
}


static void copy_value(char *dst, size_t room, const char *value) {
    size_t len = strlen(value);
    while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t')) {
        len -= 1;
    }
    if (len >= room) {
        len = room - 1;
    }
    memcpy(dst, value, len);
    dst[len] = '\0';
}


static void slot_reset_request(slot_t *slot) {
    slot->state = SLOT_REQUEST;
    slot->phase = PHASE_LINE;
    slot->buf_len = 0;
    slot->line_end = 0;
    slot->line_len = 0;
    slot->line_skip = false;
    slot->headers_total = 0;
    slot->started = false;
    slot->http10 = false;
    slot->keep_alive = false;
    slot->close_after = false;
    slot->drain = false;
    slot->has_host = false;
    slot->host[0] = '\0';
    slot->content_type[0] = '\0';
    slot->if_none_match[0] = '\0';
    slot->content_length = 0;
    slot->chunked_request = false;
    slot->expect_continue = false;
    slot->connection_close = false;
    slot->connection_keep_alive = false;
    memset(&slot->request, 0, sizeof(slot->request));
    slot->upload = NULL;
    slot->upload_left = 0;
    slot->upload_open = false;
    slot->head_len = 0;
    slot->head_sent = 0;
    slot->body_kind = BODY_NONE;
    slot->body_ptr = NULL;
    slot->body_len = 0;
    slot->body_sent = 0;
    slot->body_suppressed = false;
    slot->chunked = false;
    slot->more = NULL;
    slot->more_ctx = NULL;
    slot->more_done = false;
    slot->last_chunk_sent = false;
    slot->park_ready = NULL;
}


static void slot_close(slot_t *slot, bool abort) {
    if (slot->upload_open) {
        // An upload cut off: its handler lets go of what it began. Every way a slot ends comes here
        slot->upload_open = false;
        slot->upload->abort(slot->context);
    }
    if (slot->fd >= 0) {
        if (abort) {
            // A reset, so lwIP frees what the connection holds at once
            struct linger linger = {.l_onoff = 1, .l_linger = 0};
            lwip_setsockopt(slot->fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
        }
        lwip_close(slot->fd);
    }
    slot->fd = -1;
    slot->state = SLOT_FREE;
}


// Append to a buffer, never past its end: false once it is full
static bool append(char *out, size_t room, size_t *len, const char *fmt, ...) {
    if (*len >= room) {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int wrote = vsnprintf(out + *len, room - *len, fmt, args);
    va_end(args);
    if (wrote < 0 || (size_t) wrote >= room - *len) {
        *len = room;
        return false;
    }
    *len += (size_t) wrote;
    return true;
}


// Make the status line and headers into slot->line
static void slot_make_head(slot_t *slot, int status, const char *content_type, size_t content_length, bool chunked,
                           const char *etag, const char *location, const char *allow) {
    char *out = slot->line;
    size_t room = sizeof(slot->line);
    size_t len = 0;
    bool ok = append(out, room, &len, "HTTP/1.1 %d %s\r\n", status, status_text(status));
    if (content_type != NULL && status != 304) {
        ok = ok && append(out, room, &len, "Content-Type: %s\r\n", content_type);
    }
    if (chunked) {
        ok = ok && append(out, room, &len, "Transfer-Encoding: chunked\r\n");
    }
    else if (status != 304) {
        ok = ok && append(out, room, &len, "Content-Length: %u\r\n", (unsigned) content_length);
    }
    if (etag != NULL) {
        ok = ok && append(out, room, &len, "ETag: \"%s\"\r\nCache-Control: no-cache\r\n", etag);
    }
    else {
        ok = ok && append(out, room, &len, "Cache-Control: no-store\r\n");
    }
    if (location != NULL && location[0]) {
        ok = ok && append(out, room, &len, "Location: %s\r\n", location);
    }
    if (allow != NULL && allow[0]) {
        ok = ok && append(out, room, &len, "Allow: %s\r\n", allow);
    }
    ok = ok && append(out, room, &len, "Connection: %s\r\n\r\n", slot->keep_alive ? "keep-alive" : "close");
    if (!ok) {
        // Never a cut header: the fields above fit in 256 bytes, so this is a fault of ours
        len = 0;
        append(out, room, &len, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        slot->keep_alive = false;
        slot->body_suppressed = true;
    }
    slot->head_len = len;
    slot->head_sent = 0;
}


static void reply_init(http_server_reply_t *reply, slot_t *slot) {
    memset(reply, 0, sizeof(*reply));
    reply->slot = slot;
}


char *http_server_reply_buffer(http_server_reply_t *reply, size_t *room) {
    *room = sizeof(reply_scratch);
    return reply_scratch;
}


void http_server_reply_send(http_server_reply_t *reply, int status, const char *content_type, size_t len) {
    if (len > sizeof(reply_scratch)) {
        len = sizeof(reply_scratch);
    }
    reply->done = true;
    reply->status = status;
    reply->content_type = content_type;
    reply->kind = BODY_BUFFER;
    reply->len = len;
}


void http_server_reply_static(http_server_reply_t *reply, int status, const char *content_type, const char *data,
                              size_t len, const char *etag) {
    reply->done = true;
    reply->status = status;
    reply->content_type = content_type;
    reply->kind = BODY_STATIC;
    reply->data = data;
    reply->data_len = len;
    reply->etag = etag;
}


void http_server_reply_stream(http_server_reply_t *reply, int status, const char *content_type,
                              http_server_more_t more, void *ctx) {
    reply->done = true;
    reply->status = status;
    reply->content_type = content_type;
    reply->kind = BODY_STREAM;
    reply->more = more;
    reply->more_ctx = ctx;
}


void http_server_reply_etag(http_server_reply_t *reply, const char *etag) {
    reply->etag = etag;
}


void http_server_reply_error(http_server_reply_t *reply, int status, const char *code, const char *message) {
    int len = snprintf(reply_scratch, sizeof(reply_scratch), "{\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                       code, message);
    http_server_reply_send(reply, status, "application/json", (size_t) len);
}


void http_server_reply_raw(http_server_reply_t *reply, const char *data, size_t len) {
    if (len > sizeof(reply_scratch)) {
        len = sizeof(reply_scratch);
    }
    memmove(reply_scratch, data, len);
    reply->done = true;
    reply->raw = true;
    reply->kind = BODY_BUFFER;
    reply->len = len;
}


void http_server_reply_park(http_server_reply_t *reply, http_server_ready_t ready, void *ctx, uint32_t wait_ms,
                            http_server_handler_t resume) {
    reply->parked = true;
    slot_t *slot = reply->slot;
    slot->park_ready = ready;
    slot->park_ctx = ctx;
    slot->park_deadline_ms = now_ms() + wait_ms;
    slot->park_resume = resume;
}


bool http_server_reply_woke_ready(const http_server_reply_t *reply) {
    return reply->woke_ready;
}


void *http_server_reply_context(http_server_reply_t *reply, size_t size) {
    if (size > sizeof(reply->slot->context)) {
        return NULL;
    }
    return reply->slot->context;
}


bool http_server_query_value(const char *query, const char *name, char *out, size_t len) {
    if (query == NULL || len == 0) {
        return false;
    }
    size_t name_len = strlen(name);
    const char *pair = query;
    while (*pair) {
        const char *end = strchr(pair, '&');
        size_t pair_len = end ? (size_t) (end - pair) : strlen(pair);
        if (pair_len > name_len && strncmp(pair, name, name_len) == 0 && pair[name_len] == '=') {
            const char *src = pair + name_len + 1;
            const char *stop = pair + pair_len;
            size_t n = 0;
            while (src < stop && n + 1 < len) {
                if (src[0] == '%' && src + 2 < stop + 1 && isxdigit((unsigned char) src[1]) &&
                    isxdigit((unsigned char) src[2])) {
                    char hex[3] = {src[1], src[2], 0};
                    out[n++] = (char) strtol(hex, NULL, 16);
                    src += 3;
                }
                else {
                    out[n++] = *src == '+' ? ' ' : *src;
                    src += 1;
                }
            }
            out[n] = '\0';
            return true;
        }
        if (end == NULL) {
            break;
        }
        pair = end + 1;
    }
    return false;
}


void http_server_get_stats(http_server_stats_t *out) {
    *out = stats;
    out->stream_make_ms = (uint32_t) (stream_make_us / 1000);
    out->slots_used = 0;
    out->slots_parked = 0;
    if (slots != NULL) {
        for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
            out->slots_used += slots[idx].state != SLOT_FREE;
            out->slots_parked += slots[idx].state == SLOT_PARKED;
        }
    }
}


static int parked_count(void) {
    int count = 0;
    for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
        count += slots[idx].state == SLOT_PARKED;
    }
    return count;
}


// Turn a handler's reply into the slot's head and body, and start sending
static void slot_take_reply(slot_t *slot, http_server_reply_t *reply) {
    const http_server_request_t *request = &slot->request;
    slot->body_suppressed = request->method == HTTP_SERVER_HEAD;

    if (!reply->done) {
        http_server_reply_error(reply, 500, "failed", "the handler gave no reply");
    }
    if (reply->raw) {
        // An old handler's bytes, as they are, and the connection closed after them
        memcpy(slot->buf, reply_scratch, reply->len);
        slot->head_len = 0;
        slot->head_sent = 0;
        slot->body_kind = BODY_BUFFER;
        slot->body_ptr = slot->buf;
        slot->body_len = reply->len;
        slot->body_sent = 0;
        slot->body_suppressed = false;
        slot->keep_alive = false;
        slot->state = SLOT_SENDING;
        return;
    }

    switch (reply->kind) {
        case BODY_STATIC: {
            int status = reply->status;
            if (reply->etag != NULL && slot->if_none_match[0]) {
                // If-None-Match carries the tag in quotes, perhaps among others
                char quoted[52];
                snprintf(quoted, sizeof(quoted), "\"%s\"", reply->etag);
                if (strstr(slot->if_none_match, quoted) != NULL || strcmp(slot->if_none_match, "*") == 0) {
                    status = 304;
                    slot->body_suppressed = true;
                }
            }
            slot_make_head(slot, status, reply->content_type, reply->data_len, false, reply->etag, reply->location,
                           reply->allow);
            slot->body_kind = BODY_STATIC;
            slot->body_ptr = reply->data;
            slot->body_len = reply->data_len;
            break;
        }
        case BODY_STREAM:
            if (reply->etag != NULL && slot->if_none_match[0]) {
                // A stream with a tag (the schema, gate 3): unchanged since the client's copy, nothing is made
                char quoted[52];
                snprintf(quoted, sizeof(quoted), "\"%s\"", reply->etag);
                if (strstr(slot->if_none_match, quoted) != NULL || strcmp(slot->if_none_match, "*") == 0) {
                    slot_make_head(slot, 304, reply->content_type, 0, false, reply->etag, NULL, NULL);
                    slot->body_kind = BODY_NONE;
                    slot->body_ptr = NULL;
                    slot->body_len = 0;
                    break;
                }
            }
            if (slot->http10) {
                // No chunks for HTTP/1.0: the body ends where the connection does
                slot->keep_alive = false;
                slot->chunked = false;
            }
            else {
                slot->chunked = true;
            }
            slot_make_head(slot, reply->status, reply->content_type, 0, slot->chunked, reply->etag, NULL, NULL);
            if (slot->http10) {
                // slot_make_head wrote a Content-Length of 0; take it out by remaking the head without one
                int len = snprintf(slot->line, sizeof(slot->line),
                                   "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\n"
                                   "Connection: close\r\n\r\n", reply->status, status_text(reply->status),
                                   reply->content_type);
                slot->head_len = (size_t) len;
            }
            slot->body_kind = BODY_STREAM;
            slot->more = reply->more;
            slot->more_ctx = reply->more_ctx;
            slot->more_done = false;
            slot->last_chunk_sent = false;
            slot->body_ptr = NULL;
            slot->body_len = 0;
            break;
        case BODY_BUFFER:
        default:
            memcpy(slot->buf, reply_scratch, reply->len);
            slot_make_head(slot, reply->status, reply->content_type, reply->len, false, NULL, reply->location,
                           reply->allow);
            slot->body_kind = BODY_BUFFER;
            slot->body_ptr = slot->buf;
            slot->body_len = reply->len;
            break;
    }
    slot->body_sent = 0;
    slot->state = SLOT_SENDING;
}


// A reply the server makes itself, for a request it refuses
static void slot_refuse(slot_t *slot, int status, const char *code, const char *message) {
    http_server_reply_t reply;
    reply_init(&reply, slot);
    http_server_reply_error(&reply, status, code, message);
    stats.refused += 1;
    slot->keep_alive = false;   // what is left of the request is not read
    slot->drain = true;
    slot_take_reply(slot, &reply);
}


// The method a route is looked up by: a HEAD is a GET whose body is not sent
static http_server_method_t route_method(http_server_method_t method) {
    return method == HTTP_SERVER_HEAD ? HTTP_SERVER_GET : method;
}


// The upload route this request goes to, or NULL. The same match as slot_dispatch's, so a request for an upload's
// route never reaches slot_dispatch with its method
static const route_t *route_upload(http_server_request_t *request) {
    http_server_method_t wanted = route_method(request->method);
    for (int idx = 0; idx < route_count; idx += 1) {
        int32_t id;
        if (routes[idx].method == wanted && route_matches(&routes[idx], request->path, &id)) {
            if (routes[idx].upload == NULL) {
                return NULL;    // the first route that takes it is the one slot_dispatch would call
            }
            request->path_id = id;
            return &routes[idx];
        }
    }
    return NULL;
}


// The checks every request passes before its route. False when it was refused, with the reply made
static bool slot_admit(slot_t *slot, http_server_reply_t *reply) {
    http_server_request_t *request = &slot->request;
    const bool is_api = strncmp(request->path, "/api/", 5) == 0 || strncmp(request->path, "/rest/", 6) == 0;

    // Only the unit's own name is served: a page that has pointed its own name at the unit (DNS rebinding) is so
    // refused. A request with no Host is not a browser's
    if (slot->has_host && network.host_is_own != NULL && !network.host_is_own(slot->host)) {
        if (is_api) {
            slot_refuse(slot, 403, "wrong_host", "this is not the unit's own name");
            return false;
        }
        char address[24];
        if (network.own_address != NULL && network.own_address(address, sizeof(address))) {
            snprintf(reply->location, sizeof(reply->location), "http://%s/", address);
            http_server_reply_send(reply, 302, NULL, 0);
            slot_take_reply(slot, reply);
            return false;
        }
    }

    // A write is JSON, or bytes where a path takes them, so a browser asks the unit's leave before it sends one from
    // another site, and the unit never gives it
    const bool writes = request->method == HTTP_SERVER_POST || request->method == HTTP_SERVER_PUT ||
                        request->method == HTTP_SERVER_PATCH || request->method == HTTP_SERVER_DELETE;
    if (writes && strncmp(request->path, "/api/", 5) == 0) {
        bool json = strncasecmp(slot->content_type, "application/json", 16) == 0;
        bool bytes = request->method == HTTP_SERVER_PUT &&
                     strncasecmp(slot->content_type, "application/octet-stream", 24) == 0;
        if (!json && !bytes && request->method != HTTP_SERVER_DELETE) {
            slot_refuse(slot, 415, "wrong_media_type", "a write is sent as application/json");
            return false;
        }
        request->body_is_json = json;
    }
    return true;
}


static void slot_dispatch(slot_t *slot) {
    http_server_request_t *request = &slot->request;
    http_server_reply_t reply;
    reply_init(&reply, slot);

    if (!slot_admit(slot, &reply)) {
        return;
    }

    // The route
    http_server_method_t wanted = route_method(request->method);
    const route_t *found = NULL;
    size_t allow_len = 0;
    for (int idx = 0; idx < route_count; idx += 1) {
        int32_t id;
        if (!route_matches(&routes[idx], request->path, &id)) {
            continue;
        }
        if (routes[idx].method == wanted && found == NULL) {
            found = &routes[idx];
            request->path_id = id;
        }
        allow_len += snprintf(reply.allow + allow_len, sizeof(reply.allow) - allow_len, "%s%s",
                              allow_len ? ", " : "", method_name(routes[idx].method));
        if (allow_len >= sizeof(reply.allow)) {
            allow_len = sizeof(reply.allow) - 1;
        }
        if (routes[idx].method == HTTP_SERVER_GET && allow_len + 7 < sizeof(reply.allow)) {
            allow_len += snprintf(reply.allow + allow_len, sizeof(reply.allow) - allow_len, ", HEAD");
        }
    }
    if (found == NULL) {
        if (allow_len > 0) {
            http_server_reply_error(&reply, 405, "method_not_allowed", "the path does not take that method");
            stats.refused += 1;
        }
        else {
            reply.allow[0] = '\0';
            http_server_reply_error(&reply, 404, "not_found", "no such path on this build");
            stats.refused += 1;
        }
        slot_take_reply(slot, &reply);
        return;
    }
    reply.allow[0] = '\0';

    found->handler(request, &reply);     // never an upload's route: those are taken at their headers
    stats.requests += 1;

    if (reply.parked) {
        if (parked_count() >= HTTP_SERVER_PARK_MAX) {
            // No room to hold it: answered at once
            http_server_handler_t resume = slot->park_resume;
            http_server_reply_t now;
            reply_init(&now, slot);
            now.woke_ready = slot->park_ready(slot->park_ctx);
            slot->park_ready = NULL;
            resume(request, &now);
            slot_take_reply(slot, &now);
            return;
        }
        slot->state = SLOT_PARKED;
        return;
    }
    slot_take_reply(slot, &reply);
}


// The request line is in slot->buf: split and check it
static bool slot_parse_request_line(slot_t *slot) {
    char *line = slot->buf;
    char *sp1 = strchr(line, ' ');
    if (sp1 == NULL) {
        return false;
    }
    char *target = sp1 + 1;
    char *sp2 = strchr(target, ' ');
    if (sp2 == NULL) {
        return false;
    }
    *sp2 = '\0';
    const char *version = sp2 + 1;
    if (strcmp(version, "HTTP/1.1") == 0) {
        slot->http10 = false;
    }
    else if (strcmp(version, "HTTP/1.0") == 0) {
        slot->http10 = true;
    }
    else {
        return false;
    }
    slot->request.method = parse_method(line, (size_t) (sp1 - line));
    if (target[0] != '/') {
        return false;
    }
    char *query = strchr(target, '?');
    if (query != NULL) {
        *query = '\0';
        slot->request.query = query + 1;
    }
    else {
        slot->request.query = NULL;
    }
    decode_path(target);
    slot->request.path = target;
    slot->request.path_id = -1;
    return true;
}


static void slot_take_header(slot_t *slot) {
    const char *line = slot->line;
    const char *colon = strchr(line, ':');
    if (colon == NULL) {
        return;
    }
    size_t name_len = (size_t) (colon - line);
    const char *value = skip_spaces(colon + 1);

    if (header_is(line, name_len, "Host")) {
        slot->has_host = true;
        copy_value(slot->host, sizeof(slot->host), value);
    }
    else if (header_is(line, name_len, "Content-Length")) {
        slot->content_length = strtol(value, NULL, 10);
    }
    else if (header_is(line, name_len, "Content-Type")) {
        copy_value(slot->content_type, sizeof(slot->content_type), value);
    }
    else if (header_is(line, name_len, "Connection")) {
        if (strncasecmp(value, "close", 5) == 0) {
            slot->connection_close = true;
        }
        else if (strncasecmp(value, "keep-alive", 10) == 0) {
            slot->connection_keep_alive = true;
        }
    }
    else if (header_is(line, name_len, "If-None-Match")) {
        copy_value(slot->if_none_match, sizeof(slot->if_none_match), value);
    }
    else if (header_is(line, name_len, "Expect")) {
        slot->expect_continue = strncasecmp(value, "100-continue", 12) == 0;
    }
    else if (header_is(line, name_len, "Transfer-Encoding")) {
        slot->chunked_request = true;
    }
}


// A header the server reads, by its name at the start of a line
static bool header_is_kept(const char *line, size_t len) {
    static const char *const kept[] = {"Host", "Content-Length", "Content-Type", "Connection", "If-None-Match",
                                       "Expect", "Transfer-Encoding"};
    for (size_t idx = 0; idx < sizeof(kept) / sizeof(kept[0]); idx += 1) {
        size_t name_len = strlen(kept[idx]);
        if (len > name_len && strncasecmp(line, kept[idx], name_len) == 0 && line[name_len] == ':') {
            return true;
        }
    }
    return false;
}


// An upload's body is all in, or its handler stopped it: the handler's reply
static void slot_upload_end(slot_t *slot) {
    http_server_reply_t reply;
    reply_init(&reply, slot);
    slot->upload_open = false;
    slot->upload->end(&slot->request, &reply);
    stats.requests += 1;
    slot_take_reply(slot, &reply);
}


// The next piece of an upload's body, to its handler
static void slot_upload_piece(slot_t *slot, const char *data, size_t len) {
    if (!slot->upload->piece(slot->context, data, len)) {
        slot->keep_alive = false;   // the rest is read and dropped, then the connection closed
        slot->drain = true;
        slot_upload_end(slot);
        return;
    }
    slot->upload_left -= len;
    if (slot->upload_left == 0) {
        slot_upload_end(slot);
    }
}


// The headers of a request for an upload's route are in: its handler takes the body, or refuses it
static void slot_upload_begin(slot_t *slot, const route_t *route) {
    http_server_request_t *request = &slot->request;
    http_server_reply_t reply;
    reply_init(&reply, slot);

    // A refusal leaves the body unread: the reply says the connection closes, and the rest is read and dropped
    bool keep_alive = slot->keep_alive;
    slot->keep_alive = false;
    slot->drain = true;
    if (!slot_admit(slot, &reply)) {
        return;
    }
    request->body = NULL;
    request->body_len = (size_t) slot->content_length;
    if (!route->upload->begin(request, &reply)) {
        stats.requests += 1;
        slot_take_reply(slot, &reply);
        return;
    }
    slot->keep_alive = keep_alive;
    slot->drain = false;
    slot->upload = route->upload;
    slot->upload_left = request->body_len;
    slot->upload_open = true;
    if (slot->upload_left == 0) {
        slot_upload_end(slot);
        return;
    }
    if (slot->expect_continue) {
        static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
        lwip_send(slot->fd, cont, sizeof(cont) - 1, 0);
    }
    slot->phase = PHASE_UPLOAD;
}


// The headers are all in: decide the body and go on
static void slot_headers_done(slot_t *slot) {
    if (!slot_parse_request_line(slot)) {
        slot_refuse(slot, 400, "bad_request", "not an HTTP/1.0 or HTTP/1.1 request");
        return;
    }
    slot->keep_alive = slot->http10 ? slot->connection_keep_alive : !slot->connection_close;
    if (slot->request.method == HTTP_SERVER_OTHER || slot->request.method == HTTP_SERVER_OPTIONS) {
        // No CORS: OPTIONS is answered as no method this unit takes
        slot_refuse(slot, 405, "method_not_allowed", "the unit does not take that method");
        return;
    }
    if (slot->chunked_request) {
        slot_refuse(slot, 411, "length_required", "a body needs a Content-Length");
        return;
    }
    if (slot->content_length < 0) {
        slot_refuse(slot, 400, "bad_request", "the Content-Length is not a length");
        return;
    }
    const route_t *upload = route_upload(&slot->request);
    if (upload != NULL) {
        slot_upload_begin(slot, upload);
        return;
    }
    if (slot->content_length > HTTP_SERVER_BODY_MAX ||
        slot->line_end + 1 + (size_t) slot->content_length > sizeof(slot->buf)) {
        slot_refuse(slot, 413, "too_large", "the body is larger than the unit takes");
        return;
    }
    if (slot->content_length == 0) {
        slot->request.body = NULL;
        slot->request.body_len = 0;
        slot_dispatch(slot);
        return;
    }
    if (slot->expect_continue) {
        static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
        lwip_send(slot->fd, cont, sizeof(cont) - 1, 0);
    }
    slot->buf_len = slot->line_end + 1;
    slot->phase = PHASE_BODY;
}


// Take the bytes received, a byte at a time
static void slot_feed(slot_t *slot, const char *data, size_t len) {
    for (size_t idx = 0; idx < len; idx += 1) {
        if (slot->state != SLOT_REQUEST) {
            // A request is complete and more came behind it: no pipelining; closed once the reply is out
            slot->close_after = true;
            slot->drain = true;
            return;
        }
        char ch = data[idx];
        switch (slot->phase) {
            case PHASE_LINE:
                if (!slot->started && (ch == '\r' || ch == '\n')) {
                    break;      // an empty line between requests
                }
                slot->started = true;
                if (ch == '\n') {
                    size_t end = slot->buf_len;
                    if (end > 0 && slot->buf[end - 1] == '\r') {
                        end -= 1;
                    }
                    slot->buf[end] = '\0';
                    slot->line_end = end;
                    slot->phase = PHASE_HEADERS;
                    slot->line_len = 0;
                    break;
                }
                if (slot->buf_len >= HTTP_SERVER_REQUEST_LINE_MAX) {
                    slot_refuse(slot, 414, "too_large", "the request line is longer than the unit takes");
                    return;
                }
                slot->buf[slot->buf_len++] = ch;
                break;

            case PHASE_HEADERS:
                slot->headers_total += 1;
                if (slot->headers_total > HTTP_SERVER_HEADERS_MAX) {
                    slot_refuse(slot, 413, "too_large", "the headers are larger than the unit takes");
                    return;
                }
                if (ch == '\n') {
                    if (slot->line_skip) {
                        slot->line_skip = false;
                        slot->line_len = 0;
                        break;
                    }
                    size_t end = slot->line_len;
                    if (end > 0 && slot->line[end - 1] == '\r') {
                        end -= 1;
                    }
                    slot->line[end] = '\0';
                    slot->line_len = 0;
                    if (end == 0) {
                        slot_headers_done(slot);
                        if (slot->state != SLOT_REQUEST || slot->phase != PHASE_BODY) {
                            break;
                        }
                        break;
                    }
                    slot_take_header(slot);
                    break;
                }
                if (slot->line_skip) {
                    break;
                }
                if (slot->line_len >= sizeof(slot->line) - 1) {
                    // Too long to keep: refused if it is one the server reads, passed over if not (a long Cookie)
                    if (header_is_kept(slot->line, slot->line_len)) {
                        slot_refuse(slot, 413, "too_large", "a header the unit reads is longer than it takes");
                        return;
                    }
                    slot->line_skip = true;
                    break;
                }
                slot->line[slot->line_len++] = ch;
                break;

            case PHASE_BODY:
                slot->buf[slot->buf_len++] = ch;
                if (slot->buf_len - (slot->line_end + 1) >= (size_t) slot->content_length) {
                    slot->request.body = slot->buf + slot->line_end + 1;
                    slot->request.body_len = (size_t) slot->content_length;
                    slot_dispatch(slot);
                }
                break;

            case PHASE_UPLOAD: {
                // The rest of what came behind the headers, up to the body's end, as one piece
                size_t piece = len - idx;
                if (piece > slot->upload_left) {
                    piece = slot->upload_left;
                }
                slot_upload_piece(slot, data + idx, piece);
                idx += piece - 1;   // and the loop's own 1
                break;
            }
        }
    }
}


static void slot_receive(slot_t *slot) {
    if (slot->state == SLOT_REQUEST && slot->phase == PHASE_UPLOAD) {
        // An upload's body straight into the slot's buffer behind the request line, so a piece is as long as the
        // room there (some 900 bytes or more), and never past the body's end
        char *area = slot->buf + slot->line_end + 1;
        size_t room = sizeof(slot->buf) - (slot->line_end + 1);
        if (room > slot->upload_left) {
            room = slot->upload_left;
        }
        int got = lwip_recv(slot->fd, area, room, 0);
        if (got > 0) {
            slot->last_progress_ms = now_ms();
            slot_upload_piece(slot, area, (size_t) got);
        }
        else if (got == 0) {
            slot_close(slot, false);    // the client went before its body was all in
        }
        else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            slot_close(slot, true);
        }
        return;
    }

    char data[256];
    int got = lwip_recv(slot->fd, data, sizeof(data), 0);
    if (got > 0) {
        slot->last_progress_ms = now_ms();
        if (slot->state == SLOT_DRAINING) {
            return;     // dropped
        }
        if (slot->state == SLOT_PARKED || slot->state == SLOT_SENDING) {
            slot->close_after = true;   // more came while a reply was being made: no pipelining
            slot->drain = true;
            return;
        }
        slot_feed(slot, data, (size_t) got);
        return;
    }
    if (got == 0) {
        slot_close(slot, false);    // the client closed
        return;
    }
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
        slot_close(slot, true);
    }
}


// Make the next chunk of a streamed body into the slot's buffer. False when there is nothing more
static bool slot_next_chunk(slot_t *slot) {
    if (slot->last_chunk_sent) {
        return false;
    }
    if (slot->more_done) {
        if (!slot->chunked) {
            return false;
        }
        memcpy(slot->buf, "0\r\n\r\n", 5);
        slot->body_ptr = slot->buf;
        slot->body_len = 5;
        slot->body_sent = 0;
        slot->last_chunk_sent = true;
        return true;
    }
    size_t len = 0;
    char *data = slot->buf + HTTP_SERVER_CHUNK_HEAD;
    size_t room = sizeof(slot->buf) - HTTP_SERVER_CHUNK_HEAD - 2;
    uint64_t started_us = time_us_64();
    slot->more_done = !slot->more(slot->more_ctx, data, room, &len);
    stream_make_us += time_us_64() - started_us;
    stats.stream_bytes += (uint32_t) len;
    if (len == 0) {
        return slot_next_chunk(slot);
    }
    if (!slot->chunked) {
        slot->body_ptr = data;
        slot->body_len = len;
        slot->body_sent = 0;
        return true;
    }
    char head[HTTP_SERVER_CHUNK_HEAD + 1];
    int head_len = snprintf(head, sizeof(head), "%x\r\n", (unsigned) len);
    char *start = data - head_len;
    memcpy(start, head, (size_t) head_len);
    data[len] = '\r';
    data[len + 1] = '\n';
    slot->body_ptr = start;
    slot->body_len = (size_t) head_len + len + 2;
    slot->body_sent = 0;
    return true;
}


// A streamed reply makes this many chunks a pass, then the other clients have their turn. On a good Wi-Fi the ACKs
// come back faster than chunks are made, and a stream that went on while lwIP took its bytes held the server for its
// whole length (1.8 s for a throw's samples), every other request waiting behind it. One chunk a pass is one segment
// in flight, whose ACK the client delays: the stream took 4 s. Two fill the send buffer (TCP_SND_BUF, 2 segments)
#define HTTP_SERVER_CHUNKS_PER_PASS 2

// Send what the socket takes. True when the reply is all out
static bool slot_send_some(slot_t *slot) {
    bool sent_any = false;
    int chunks_made = 0;
    while (true) {
        const char *ptr;
        size_t left;
        if (slot->head_sent < slot->head_len) {
            ptr = slot->line + slot->head_sent;
            left = slot->head_len - slot->head_sent;
        }
        else if (slot->body_suppressed || slot->body_kind == BODY_NONE) {
            return true;
        }
        else if (slot->body_sent < slot->body_len) {
            ptr = slot->body_ptr + slot->body_sent;
            left = slot->body_len - slot->body_sent;
        }
        else if (slot->body_kind == BODY_STREAM && chunks_made < HTTP_SERVER_CHUNKS_PER_PASS &&
                 slot_next_chunk(slot)) {
            chunks_made += 1;
            continue;
        }
        else if (slot->body_kind == BODY_STREAM && chunks_made > 0 && !slot->last_chunk_sent) {
            return false;   // the next chunks on the next pass, after the others
        }
        else {
            return true;
        }

        int sent = lwip_send(slot->fd, ptr, left, MSG_DONTWAIT);
        if (sent > 0) {
            sent_any = true;
            slot->last_progress_ms = now_ms();
            if (slot->head_sent < slot->head_len) {
                slot->head_sent += (size_t) sent;
            }
            else {
                slot->body_sent += (size_t) sent;
            }
            continue;
        }
        if (sent < 0 && (errno == ENOMEM || errno == EWOULDBLOCK || errno == EAGAIN)) {
            if (!sent_any) {
                // Nothing went, though select() said writable: select() looks at the send window, not at lwIP's
                // heap, and a non-blocking send that finds no heap says "would block" (api_msg.c). Wait a tick, or
                // the task would spin
                stats.send_no_memory += 1;
                slot->no_memory_until_ms = now_ms() + HTTP_SERVER_NO_MEMORY_WAIT_MS;
            }
            return false;
        }
        stats.send_failed += 1;
        printf("HTTP: a send failed (%d, errno %d) after %u bytes of the body; the connection is closed\n", sent,
               sent < 0 ? errno : 0, (unsigned) slot->body_sent);
        slot_close(slot, true);
        return false;
    }
}


static void slot_after_send(slot_t *slot) {
    if (slot->keep_alive && !slot->close_after) {
        slot_reset_request(slot);
        slot->last_progress_ms = now_ms();
    }
    else if (slot->drain) {
        // Closed with the client's bytes unread, lwIP answers with a reset, which can reach the client before the
        // reply it has not read yet (a 413 lost so). Stop sending, read what is left, and close at its end
        lwip_shutdown(slot->fd, SHUT_WR);
        slot->state = SLOT_DRAINING;
        slot->drain_until_ms = now_ms() + HTTP_SERVER_DRAIN_MS;
    }
    else {
        slot_close(slot, false);
    }
}


static void slot_resume_parked(slot_t *slot, bool ready) {
    http_server_reply_t reply;
    reply_init(&reply, slot);
    reply.woke_ready = ready;
    http_server_handler_t resume = slot->park_resume;
    slot->park_ready = NULL;
    slot->state = SLOT_REQUEST;     // so slot_take_reply sends it
    resume(&slot->request, &reply);
    slot_take_reply(slot, &reply);
}


static int server_listen(void) {
    int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int on = 1;
    lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(HTTP_SERVER_PORT);
    addr.sin_addr.s_addr = PP_HTONL(INADDR_ANY);
    if (lwip_bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0 || lwip_listen(fd, HTTP_SERVER_SLOTS) < 0) {
        lwip_close(fd);
        return -1;
    }
    lwip_fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}


// A kept-alive connection waiting for its next request, idle longest: closed to make room
static slot_t *slot_idlest(void) {
    slot_t *idlest = NULL;
    for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
        slot_t *slot = &slots[idx];
        if (slot->state == SLOT_REQUEST && !slot->started &&
            (idlest == NULL || (int32_t) (slot->last_progress_ms - idlest->last_progress_ms) < 0)) {
            idlest = slot;
        }
    }
    return idlest;
}


static slot_t *slot_free(void) {
    for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
        if (slots[idx].state == SLOT_FREE) {
            return &slots[idx];
        }
    }
    return NULL;
}


static void server_accept(int listen_fd) {
    slot_t *slot = slot_free();
    if (slot == NULL) {
        slot = slot_idlest();
        if (slot == NULL) {
            return;
        }
        stats.closed_for_room += 1;
        slot_close(slot, false);
    }
    int fd = lwip_accept(listen_fd, NULL, NULL);
    if (fd < 0) {
        return;
    }
    lwip_fcntl(fd, F_SETFL, O_NONBLOCK);
    slot->fd = fd;
    slot_reset_request(slot);
    slot->last_progress_ms = now_ms();
    slot->no_memory_until_ms = 0;
    stats.accepted += 1;
}


static void server_timers(void) {
    uint32_t now = now_ms();
    for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
        slot_t *slot = &slots[idx];
        uint32_t quiet = now - slot->last_progress_ms;
        switch (slot->state) {
            case SLOT_REQUEST:
                if (!slot->started && quiet > HTTP_SERVER_IDLE_MS) {
                    stats.closed_idle += 1;
                    slot_close(slot, false);
                }
                else if (slot->started && quiet > HTTP_SERVER_STALL_MS) {
                    stats.aborted_stalled += 1;
                    slot_close(slot, true);
                }
                break;
            case SLOT_PARKED:
                if (slot->park_ready(slot->park_ctx)) {
                    slot_resume_parked(slot, true);
                }
                else if ((int32_t) (now - slot->park_deadline_ms) >= 0) {
                    slot_resume_parked(slot, false);
                }
                break;
            case SLOT_SENDING:
                // A reply that makes no progress while there is something to send: lwIP frees what it holds
                if (quiet > HTTP_SERVER_STALL_MS) {
                    stats.aborted_stalled += 1;
                    slot_close(slot, true);
                }
                break;
            case SLOT_DRAINING:
                if ((int32_t) (now - slot->drain_until_ms) >= 0) {
                    slot_close(slot, true);
                }
                break;
            default:
                break;
        }
    }
}


static void server_task(void *arg) {
    int listen_fd = -1;
    while (listen_fd < 0) {
        listen_fd = server_listen();
        if (listen_fd < 0) {
            printf("HTTP: could not listen on port %d; trying again\n", HTTP_SERVER_PORT);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    printf("HTTP: our own server listening on port %d (ADR-003)\n", HTTP_SERVER_PORT);

    while (true) {
        fd_set read_set;
        fd_set write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        int max_fd = -1;
        uint32_t now = now_ms();
        bool waiting_for_memory = false;

        // The listener only when a slot is free or can be freed, or select() would wake at once and again
        if (slot_free() != NULL || slot_idlest() != NULL) {
            FD_SET(listen_fd, &read_set);
            max_fd = listen_fd;
        }
        for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
            slot_t *slot = &slots[idx];
            if (slot->state == SLOT_FREE) {
                continue;
            }
            FD_SET(slot->fd, &read_set);    // a request's bytes, or a client that closes
            if (slot->state == SLOT_SENDING) {
                if ((int32_t) (now - slot->no_memory_until_ms) >= 0) {
                    FD_SET(slot->fd, &write_set);
                }
                else {
                    waiting_for_memory = true;
                }
            }
            if (slot->fd > max_fd) {
                max_fd = slot->fd;
            }
        }

        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = (waiting_for_memory ? HTTP_SERVER_NO_MEMORY_WAIT_MS : HTTP_SERVER_TICK_MS) * 1000,
        };
        int ready = max_fd >= 0 ? lwip_select(max_fd + 1, &read_set, &write_set, NULL, &timeout) : 0;
        if (max_fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_TICK_MS));
        }

        if (ready > 0) {
            if (FD_ISSET(listen_fd, &read_set)) {
                server_accept(listen_fd);
            }
            for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
                slot_t *slot = &slots[idx];
                if (slot->state != SLOT_FREE && FD_ISSET(slot->fd, &read_set)) {
                    slot_receive(slot);
                }
            }
        }

        // Send what can be sent: a reply made just now goes out without waiting for the next select()
        now = now_ms();
        for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
            slot_t *slot = &slots[idx];
            if (slot->state == SLOT_SENDING && (int32_t) (now - slot->no_memory_until_ms) >= 0) {
                if (slot_send_some(slot) && slot->state == SLOT_SENDING) {
                    slot_after_send(slot);
                }
            }
        }
        server_timers();
    }
}


bool http_server_start(const http_server_network_t *net) {
    if (server_task_handle != NULL) {
        return true;
    }
    if (net != NULL) {
        network = *net;
    }
    // The slots come out of the FreeRTOS heap, which has room, not out of the RAM taken at link time
    slots = pvPortMalloc(sizeof(slot_t) * HTTP_SERVER_SLOTS);
    if (slots == NULL) {
        printf("HTTP: no room for the server's slots\n");
        return false;
    }
    for (int idx = 0; idx < HTTP_SERVER_SLOTS; idx += 1) {
        memset(&slots[idx], 0, sizeof(slot_t));
        slots[idx].fd = -1;
        slots[idx].state = SLOT_FREE;
    }
    return xTaskCreate(server_task, "HTTP Server", HTTP_SERVER_TASK_STACK_WORDS, NULL, HTTP_SERVER_TASK_PRIORITY,
                       &server_task_handle) == pdPASS;
}
