/*
 * A GET by the unit itself, over HTTPS or plain HTTP (ADR-010, gate 5; https_client.h). Grown from gate 1's measure
 * (measure_tls.c), whose figures it keeps: every mbedTLS allocation is taken from the FreeRTOS heap and counted.
 *
 * Measured with gate 4's firmware (progress.md, 2026-11-17): a fetch through a redirect, an ECDSA chain then an RSA
 * one, took 48.5 KB of the FreeRTOS heap with a task of its own on an 8 KB stack, of which it used 2.4 KB; mbedTLS's
 * peak was 38.8 KB, 40.7 KB with GitHub's chains. The handshakes took 6.2 s and 2.3 s on the Pico W.
 */
#include "https_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "chip.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include "https_ca.h"
#include "version.h"

#define HTTPS_URL_MAX 1024          // a redirect to GitHub's asset host carries a long signed query
#define HTTPS_HOST_MAX 128
#define HTTPS_TIMEOUT_S 15          // a read that waits longer than this fails
#define HTTPS_ALLOC_HEADER 8        // each mbedTLS block carries its size ahead of it, so what it holds is counted
#define HTTPS_HEAP_RESERVE 2048     // what a fetch leaves in the FreeRTOS heap for the rest of the unit, at the least

typedef struct {
    bool tls;
    char host[HTTPS_HOST_MAX];
    char port[8];
    const char *path;
} https_url_t;

typedef struct {
    int fd;
    bool tls;
} https_conn_t;

// One fetch at a time, under the lock: these are its
static SemaphoreHandle_t https_lock = NULL;
// The hop's address, then the next's from its redirect: nothing reads the one once its request is sent
static char https_url[HTTPS_URL_MAX];
static mbedtls_ssl_context https_ssl;
static mbedtls_ssl_config https_config;
static mbedtls_x509_crt https_ca;
static mbedtls_entropy_context https_entropy;
static mbedtls_ctr_drbg_context https_drbg;
static uint8_t https_rx[512];
static size_t https_rx_len;
static size_t https_rx_pos;
static char https_line[HTTPS_URL_MAX + 160];    // the request, then each header line as it is read
static https_report_t https_report;
static size_t https_in_use;                     // what mbedTLS holds now


// --- mbedTLS's allocations, counted

static void *https_calloc(size_t count, size_t size) {
    if (size != 0 && count > (SIZE_MAX - HTTPS_ALLOC_HEADER) / size) {
        return NULL;
    }
    size_t bytes = count * size;
    // The fetch is the heap's one large, optional user: it gives way, so a heap short of room ends the fetch with a
    // reason (mbedTLS's "no memory") and never the unit (the hook's assertion, as the Pico 2 W's poll did, ADR-008)
    if (xPortGetFreeHeapSize() < bytes + HTTPS_ALLOC_HEADER + HTTPS_HEAP_RESERVE) {
        https_report.alloc_refused += 1;
        return NULL;
    }
    uint8_t *block = pvPortMalloc(bytes + HTTPS_ALLOC_HEADER);
    if (block == NULL) {
        return NULL;
    }
    memcpy(block, &bytes, sizeof(bytes));
    memset(block + HTTPS_ALLOC_HEADER, 0, bytes);
    https_in_use += bytes;
    if (https_in_use > https_report.mbedtls_peak) {
        https_report.mbedtls_peak = (uint32_t) https_in_use;
    }
    size_t left = xPortGetFreeHeapSize();
    if (https_report.heap_low == 0 || left < https_report.heap_low) {
        https_report.heap_low = (uint32_t) left;
    }
    return block + HTTPS_ALLOC_HEADER;
}


static void https_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    uint8_t *block = (uint8_t *) ptr - HTTPS_ALLOC_HEADER;
    size_t bytes;
    memcpy(&bytes, block, sizeof(bytes));
    https_in_use -= bytes;
    vPortFree(block);
}


bool https_client_init(void) {
    https_lock = xSemaphoreCreateMutex();
    mbedtls_platform_set_calloc_free(https_calloc, https_free);
    return https_lock != NULL;
}


// --- The connection

static void https_fail(const char *what, int code) {
    if (https_report.alloc_refused > 0) {
        // A block refused at the reserve: what failed after it failed for that, whatever mbedTLS made of it (a
        // certificate it could not hold reads as one not trusted)
        char after[sizeof(https_report.error) - sizeof("no room in memory ()")];
        snprintf(after, sizeof(after), "%s", what);
        snprintf(https_report.error, sizeof(https_report.error), "no room in memory (%s)", after);
    }
    else if (code != 0) {
        snprintf(https_report.error, sizeof(https_report.error), "%s (-0x%04x)", what, (unsigned) -code);
    }
    else {
        snprintf(https_report.error, sizeof(https_report.error), "%s", what);
    }
}


static bool https_parse_url(const char *text, https_url_t *url) {
    const char *rest;
    if (strncmp(text, "https://", 8) == 0) {
        url->tls = true;
        rest = text + 8;
    }
    else if (strncmp(text, "http://", 7) == 0) {
        url->tls = false;
        rest = text + 7;
    }
    else {
        return false;
    }
    size_t host_end = strcspn(rest, ":/?");
    if (host_end == 0 || host_end >= sizeof(url->host)) {
        return false;
    }
    memcpy(url->host, rest, host_end);
    url->host[host_end] = '\0';
    rest += host_end;
    if (*rest == ':') {
        size_t port_len = strcspn(rest + 1, "/?");
        if (port_len == 0 || port_len >= sizeof(url->port)) {
            return false;
        }
        memcpy(url->port, rest + 1, port_len);
        url->port[port_len] = '\0';
        rest += 1 + port_len;
    }
    else {
        snprintf(url->port, sizeof(url->port), "%s", url->tls ? "443" : "80");
    }
    url->path = *rest == '/' ? rest : "/";
    return true;
}


static int https_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    int sent = lwip_send(*(int *) ctx, buf, len, 0);
    return sent >= 0 ? sent : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}


static int https_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    int got = lwip_recv(*(int *) ctx, buf, len, 0);
    return got >= 0 ? got : MBEDTLS_ERR_SSL_TIMEOUT;
}


static int https_connect(const https_url_t *url) {
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
    struct addrinfo *found = NULL;
    if (lwip_getaddrinfo(url->host, url->port, &hints, &found) != 0 || found == NULL) {
        https_fail("no address for the host", 0);
        return -1;
    }
    int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || lwip_connect(fd, found->ai_addr, found->ai_addrlen) != 0) {
        lwip_freeaddrinfo(found);
        if (fd >= 0) {
            lwip_close(fd);
        }
        https_fail("no connection", 0);
        return -1;
    }
    lwip_freeaddrinfo(found);
    struct timeval timeout = {.tv_sec = HTTPS_TIMEOUT_S};
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}


static bool https_handshake(https_conn_t *conn, const https_url_t *url, int hop) {
    mbedtls_ssl_init(&https_ssl);
    int ret = mbedtls_ssl_setup(&https_ssl, &https_config);
    if (ret == 0) {
        // The name the certificate must carry, and the server's name sent (SNI)
        ret = mbedtls_ssl_set_hostname(&https_ssl, url->host);
    }
    if (ret != 0) {
        https_fail("no room for the session", ret);
        return false;
    }
    mbedtls_ssl_set_bio(&https_ssl, &conn->fd, https_bio_send, https_bio_recv, NULL);

    uint64_t started = chip_time_us();
    while ((ret = mbedtls_ssl_handshake(&https_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            uint32_t flags = mbedtls_ssl_get_verify_result(&https_ssl);
            snprintf(https_report.error, sizeof(https_report.error), "the handshake failed (-0x%04x), verify 0x%lx",
                     (unsigned) -ret, (unsigned long) flags);
            if (https_report.alloc_refused > 0) {
                https_fail(https_report.error, 0);  // said as no room, which was the cause (it copies first)
            }
            return false;
        }
    }
    https_report.handshake_ms[hop] = (uint32_t) ((chip_time_us() - started) / 1000);
    return true;
}


static void https_close(https_conn_t *conn) {
    if (conn->tls) {
        mbedtls_ssl_close_notify(&https_ssl);
        mbedtls_ssl_free(&https_ssl);
    }
    lwip_close(conn->fd);
}


static int https_write(https_conn_t *conn, const char *text) {
    size_t len = strlen(text);
    if (!conn->tls) {
        return lwip_send(conn->fd, text, len, 0) == (int) len ? 0 : -1;
    }
    while (len > 0) {
        int sent = mbedtls_ssl_write(&https_ssl, (const unsigned char *) text, len);
        if (sent < 0) {
            return sent;
        }
        text += sent;
        len -= (size_t) sent;
    }
    return 0;
}


// The read buffer filled again once it is used up: the bytes in it, 0 at the stream's end, below 0 on an error
static int https_fill(https_conn_t *conn) {
    if (https_rx_pos < https_rx_len) {
        return (int) (https_rx_len - https_rx_pos);
    }
    int got;
    if (conn->tls) {
        got = mbedtls_ssl_read(&https_ssl, https_rx, sizeof(https_rx));
        if (got == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || got == MBEDTLS_ERR_SSL_CONN_EOF) {
            got = 0;
        }
    }
    else {
        got = lwip_recv(conn->fd, https_rx, sizeof(https_rx), 0);
    }
    https_rx_len = got > 0 ? (size_t) got : 0;
    https_rx_pos = 0;
    return got;
}


// One line, its CR LF dropped; a line longer than the buffer is cut. False when the stream ends first
static bool https_read_line(https_conn_t *conn, char *line, size_t size) {
    size_t len = 0;
    while (true) {
        if (https_fill(conn) <= 0) {
            return false;
        }
        char c = (char) https_rx[https_rx_pos++];
        if (c == '\n') {
            break;
        }
        if (c != '\r' && len + 1 < size) {
            line[len++] = c;
        }
    }
    line[len] = '\0';
    return true;
}


// So many bytes of the body to the sink, as they come. False when the stream ends first or the sink stops it
static bool https_take(https_conn_t *conn, size_t count, const https_sink_t *sink) {
    while (count > 0) {
        int avail = https_fill(conn);
        if (avail <= 0) {
            https_fail("the body was cut short", avail);
            return false;
        }
        size_t take = (size_t) avail < count ? (size_t) avail : count;
        if (!sink->piece(sink->ctx, https_rx + https_rx_pos, take)) {
            https_fail("stopped", 0);
            return false;
        }
        https_rx_pos += take;
        https_report.bytes += (uint32_t) take;
        count -= take;
    }
    return true;
}


// The body: to its length, to the stream's end when none is given, or chunk by chunk
static bool https_read_body(https_conn_t *conn, long length, bool chunked, const https_sink_t *sink) {
    if (!chunked) {
        if (length >= 0) {
            return https_take(conn, (size_t) length, sink);
        }
        while (true) {
            int avail = https_fill(conn);
            if (avail == 0) {
                return true;
            }
            if (avail < 0) {
                https_fail("the body was cut short", avail);
                return false;
            }
            if (!https_take(conn, (size_t) avail, sink)) {
                return false;
            }
        }
    }
    while (true) {
        // A chunk's size in hex, then its bytes and a CR LF; a size of 0 ends the body, then its trailers
        if (!https_read_line(conn, https_line, sizeof(https_line))) {
            https_fail("the body was cut short", 0);
            return false;
        }
        char *end = NULL;
        unsigned long size = strtoul(https_line, &end, 16);
        if (end == https_line) {
            https_fail("a chunk without its size", 0);
            return false;
        }
        if (size == 0) {
            while (https_read_line(conn, https_line, sizeof(https_line)) && https_line[0] != '\0') {
            }
            return true;
        }
        if (!https_take(conn, size, sink) || !https_read_line(conn, https_line, sizeof(https_line))) {
            return false;
        }
    }
}


/*
 * One request: either the body to the sink, or the next URL taken from a redirect into https_url, in place of this
 * one's, which is not read once the request is sent. False on a failure, its reason in the report
 */
static bool https_hop(int hop, bool *redirected, const https_sink_t *sink) {
    char *next = https_url;
    https_url_t url;
    if (!https_parse_url(https_url, &url)) {
        https_fail("not an http or https URL", 0);
        return false;
    }
    https_conn_t conn = {.fd = https_connect(&url), .tls = url.tls};
    if (conn.fd < 0) {
        return false;
    }
    if (url.tls && !https_handshake(&conn, &url, hop)) {
        lwip_close(conn.fd);
        mbedtls_ssl_free(&https_ssl);
        return false;
    }

    // The port is named in Host only when it is not the scheme's own. GitHub's API asks for a User-Agent
    bool own_port = strcmp(url.port, url.tls ? "443" : "80") == 0;
    snprintf(https_line, sizeof(https_line), "GET %s HTTP/1.1\r\nHost: %s%s%s\r\nUser-Agent: OpenTrickler/%s\r\n"
             "Accept: */*\r\nConnection: close\r\n\r\n", url.path, url.host, own_port ? "" : ":",
             own_port ? "" : url.port, version_string);
    if (https_write(&conn, https_line) != 0) {
        https_fail("the request was not sent", 0);
        https_close(&conn);
        return false;
    }

    https_rx_len = 0;
    https_rx_pos = 0;
    // The status after the version ("HTTP/1.1 302 Found"). Not sscanf: newlib's brings 30 KB of wide-character tables
    const char *status = NULL;
    if (!https_read_line(&conn, https_line, sizeof(https_line)) || strncmp(https_line, "HTTP/", 5) != 0 ||
        (status = strchr(https_line, ' ')) == NULL || (https_report.status = (int) strtol(status, NULL, 10)) <= 0) {
        https_fail("no status line", 0);
        https_close(&conn);
        return false;
    }
    long length = -1;
    bool chunked = false;
    *redirected = false;
    while (https_read_line(&conn, https_line, sizeof(https_line)) && https_line[0] != '\0') {
        if (strncasecmp(https_line, "content-length:", 15) == 0) {
            length = strtol(https_line + 15, NULL, 10);
        }
        else if (strncasecmp(https_line, "transfer-encoding:", 18) == 0 && strstr(https_line + 18, "chunked") != NULL) {
            chunked = true;
        }
        else if (strncasecmp(https_line, "location:", 9) == 0 && https_report.status / 100 == 3) {
            const char *value = https_line + 9;
            while (*value == ' ') {
                value += 1;
            }
            if (value[0] == '/') {
                // A path on the same host
                snprintf(next, HTTPS_URL_MAX, "%s://%s:%s%s", url.tls ? "https" : "http", url.host, url.port, value);
            }
            else {
                snprintf(next, HTTPS_URL_MAX, "%s", value);
            }
            *redirected = true;
        }
    }

    bool ok = true;
    if (!*redirected) {
        https_report.bytes = 0;
        if (!sink->begin(sink->ctx, https_report.status, length)) {
            https_fail("stopped", 0);
            ok = false;
        }
        else {
            ok = https_read_body(&conn, length, chunked, sink);
        }
    }
    https_close(&conn);
    return ok;
}


static bool https_setup(void) {
    mbedtls_x509_crt_init(&https_ca);
    mbedtls_ssl_config_init(&https_config);
    mbedtls_entropy_init(&https_entropy);
    mbedtls_ctr_drbg_init(&https_drbg);

    static const unsigned char personal[] = "opentrickler";
    int ret = mbedtls_ctr_drbg_seed(&https_drbg, mbedtls_entropy_func, &https_entropy, personal, sizeof(personal) - 1);
    if (ret != 0) {
        https_fail("no random numbers", ret);
        return false;
    }
    ret = mbedtls_x509_crt_parse(&https_ca, (const unsigned char *) https_ca_pem, sizeof(https_ca_pem));
    if (ret != 0) {
        https_fail("the roots did not parse", ret);
        return false;
    }
    ret = mbedtls_ssl_config_defaults(&https_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        https_fail("no TLS configuration", ret);
        return false;
    }
    mbedtls_ssl_conf_authmode(&https_config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&https_config, &https_ca, NULL);
    mbedtls_ssl_conf_rng(&https_config, mbedtls_ctr_drbg_random, &https_drbg);
    return true;
}


static void https_teardown(void) {
    mbedtls_ssl_config_free(&https_config);
    mbedtls_x509_crt_free(&https_ca);
    mbedtls_ctr_drbg_free(&https_drbg);
    mbedtls_entropy_free(&https_entropy);
}


bool https_get(const char *url, const https_sink_t *sink) {
    if (https_lock == NULL || xSemaphoreTake(https_lock, 0) != pdTRUE) {
        return false;
    }
    memset(&https_report, 0, sizeof(https_report));
    https_report.mbedtls_peak = (uint32_t) https_in_use;
    https_report.heap_free_before = (uint32_t) xPortGetFreeHeapSize();
    uint64_t started = chip_time_us();

    bool ok = strlen(url) < HTTPS_URL_MAX;
    if (!ok) {
        https_fail("the URL is too long", 0);
    }
    else {
        snprintf(https_url, sizeof(https_url), "%s", url);
        ok = https_setup();
    }
    int hop = 0;
    while (ok) {
        bool redirected = false;
        ok = https_hop(hop, &redirected, sink);
        hop += 1;
        if (!ok || !redirected) {
            break;
        }
        if (hop == HTTPS_MAX_HOPS) {
            https_fail("too many redirects", 0);
            ok = false;
        }
    }
    https_teardown();

    https_report.hops = (uint8_t) hop;
    https_report.total_ms = (uint32_t) ((chip_time_us() - started) / 1000);
    https_report.heap_min_after = (uint32_t) xPortGetMinimumEverFreeHeapSize();
    https_report.stack_left = (uint32_t) (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    xSemaphoreGive(https_lock);
    return ok;
}


void https_last_report(https_report_t *report) {
    *report = https_report;
}
