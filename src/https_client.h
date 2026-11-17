#ifndef HTTPS_CLIENT_H_
#define HTTPS_CLIENT_H_

/*
 * A GET by the unit itself, over HTTPS or plain HTTP (ADR-010, gate 5): the update's poll and its download. mbedTLS
 * over lwIP's sockets, run in the caller's task, so it takes no task of its own (gate 1's trim). Up to three
 * redirects are followed, and the body is handed over in pieces as it comes, a chunked one unchunked. The roots
 * trusted are built in (https_ca.h, made by CMakeLists.txt): GitHub's two, and the local OTA server's test roots
 * only in a build that names them. The unit has no clock, so a certificate's dates are not checked.
 *
 * One fetch at a time: a second, asked while one runs, fails at once with "busy".
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTPS_MAX_HOPS 4

// Where the last hop's body goes. Either function returning false stops the fetch, and it fails with the reason
typedef struct {
    // Once its headers are in: its HTTP status, and its length (-1 when not given)
    bool (*begin)(void *ctx, int status, long length);
    bool (*piece)(void *ctx, const uint8_t *data, size_t len);
    void *ctx;
} https_sink_t;

// How the last fetch went: what ADR-010's gate 1 measured, for the console
typedef struct {
    int status;                             // the last hop's HTTP status
    uint32_t bytes;                         // its body's
    uint8_t hops;
    uint32_t handshake_ms[HTTPS_MAX_HOPS];  // each hop's TLS handshake; 0 over plain HTTP
    uint32_t total_ms;
    uint32_t mbedtls_peak;                  // the most mbedTLS held at once, in bytes
    uint32_t heap_free_before;              // the FreeRTOS heap free when it began
    uint32_t heap_min_after;                // the heap's low mark after it, over the unit's whole run
    uint32_t stack_left;                    // the calling task's stack never touched, in bytes
    char error[96];                         // "" when it went through
} https_report_t;

#ifdef __cplusplus
extern "C" {
#endif

// Once at start-up, before any fetch
bool https_client_init(void);

/*
 * GET the URL ("https://host[:port]/path" or "http://..."), following redirects, the body to the sink. True when
 * the last hop answered and its body was taken whole; false, with why in the report's `error`, otherwise. A status
 * other than 200 is the sink's to judge.
 */
bool https_get(const char *url, const https_sink_t *sink);

// The last fetch's figures
void https_last_report(https_report_t *report);

#ifdef __cplusplus
}
#endif

#endif  // HTTPS_CLIENT_H_
