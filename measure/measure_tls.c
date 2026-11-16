/*
    ADR-010, gate 1: one file fetched by the unit over HTTPS, to measure what TLS takes. mbedTLS over lwIP's sockets,
    in a task of its own (as a download job would run, not in lwIP's thread), every mbedTLS allocation taken from the
    FreeRTOS heap and counted. The console's:
      tls get <url>   starts one fetch, http or https, following up to three redirects; the body is read and counted,
                      not kept. It answers at once; the fetch logs a `Measure: tls` line when it ends
      tls             how the last fetch went: its status, bytes, each hop's handshake, mbedTLS's peak and what it
                      still holds, the FreeRTOS heap before and its low mark, the task's stack left
    The unit trusts the roots built in (measure_ca.h, made from ADR010_MEASURE_CA): the local OTA server's test roots
    and GitHub's. It has no clock, so a certificate's dates are not checked.
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/time.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include "measure_ca.h"
#include "measure_tls.h"

#define MEASURE_TLS_TASK_STACK_WORDS 2048
#define MEASURE_TLS_TASK_PRIORITY 3
#define MEASURE_TLS_MAX_HOPS 4
#define MEASURE_TLS_URL_MAX 1024
#define MEASURE_TLS_HOST_MAX 128
#define MEASURE_TLS_TIMEOUT_S 15
#define MEASURE_TLS_HEADER_SIZE 8

typedef enum {
    MEASURE_TLS_IDLE,
    MEASURE_TLS_RUNNING,
    MEASURE_TLS_DONE,
    MEASURE_TLS_FAILED,
} measure_tls_state_t;

typedef struct {
    bool tls;
    char host[MEASURE_TLS_HOST_MAX];
    char port[8];
    const char *path;
} measure_tls_url_t;

typedef struct {
    int fd;
    bool tls;
} measure_tls_conn_t;

static const char *measure_tls_state_names[] = {"idle", "running", "done", "failed"};

// The fetch's own state: one fetch at a time
static volatile measure_tls_state_t measure_tls_state = MEASURE_TLS_IDLE;
static char measure_tls_urls[2][MEASURE_TLS_URL_MAX];
static mbedtls_ssl_context measure_tls_ssl;
static mbedtls_ssl_config measure_tls_config;
static mbedtls_x509_crt measure_tls_ca;
static mbedtls_entropy_context measure_tls_entropy;
static mbedtls_ctr_drbg_context measure_tls_drbg;
static uint8_t measure_tls_rx[512];
static size_t measure_tls_rx_len;
static size_t measure_tls_rx_pos;
static char measure_tls_request[MEASURE_TLS_URL_MAX + 160];
static char measure_tls_line[MEASURE_TLS_URL_MAX + 16];

// What `tls` reports
static int measure_tls_status;
static uint32_t measure_tls_bytes;
static int measure_tls_hops;
static uint32_t measure_tls_handshake_ms[MEASURE_TLS_MAX_HOPS];
static uint32_t measure_tls_total_ms;
static char measure_tls_error[96];
static size_t measure_tls_heap_before;
static size_t measure_tls_heap_min_before;
static size_t measure_tls_heap_min_after;
static uint32_t measure_tls_stack_left;

// mbedTLS's allocations, counted: each block carries its size ahead of it
static size_t measure_tls_in_use;
static size_t measure_tls_peak;
static uint32_t measure_tls_allocs;
static uint32_t measure_tls_alloc_failed;


static void *measure_tls_calloc(size_t count, size_t size) {
    if (size != 0 && count > (SIZE_MAX - MEASURE_TLS_HEADER_SIZE) / size) {
        measure_tls_alloc_failed += 1;
        return NULL;
    }
    size_t bytes = count * size;
    uint8_t *block = pvPortMalloc(bytes + MEASURE_TLS_HEADER_SIZE);
    if (block == NULL) {
        measure_tls_alloc_failed += 1;
        return NULL;
    }
    memcpy(block, &bytes, sizeof(bytes));
    memset(block + MEASURE_TLS_HEADER_SIZE, 0, bytes);
    measure_tls_allocs += 1;
    measure_tls_in_use += bytes;
    if (measure_tls_in_use > measure_tls_peak) {
        measure_tls_peak = measure_tls_in_use;
    }
    return block + MEASURE_TLS_HEADER_SIZE;
}


static void measure_tls_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    uint8_t *block = (uint8_t *) ptr - MEASURE_TLS_HEADER_SIZE;
    size_t bytes;
    memcpy(&bytes, block, sizeof(bytes));
    measure_tls_in_use -= bytes;
    vPortFree(block);
}


static bool measure_tls_parse_url(const char *text, measure_tls_url_t *url) {
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
    size_t host_end = strcspn(rest, ":/");
    if (host_end == 0 || host_end >= sizeof(url->host)) {
        return false;
    }
    memcpy(url->host, rest, host_end);
    url->host[host_end] = '\0';
    rest += host_end;
    if (*rest == ':') {
        size_t port_len = strcspn(rest + 1, "/");
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


static int measure_tls_send(void *ctx, const unsigned char *buf, size_t len) {
    int sent = lwip_send(*(int *) ctx, buf, len, 0);
    return sent >= 0 ? sent : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}


static int measure_tls_recv(void *ctx, unsigned char *buf, size_t len) {
    int got = lwip_recv(*(int *) ctx, buf, len, 0);
    return got >= 0 ? got : MBEDTLS_ERR_SSL_TIMEOUT;
}


static int measure_tls_write(measure_tls_conn_t *conn, const char *text) {
    size_t len = strlen(text);
    if (!conn->tls) {
        return lwip_send(conn->fd, text, len, 0) == (int) len ? 0 : -1;
    }
    while (len > 0) {
        int sent = mbedtls_ssl_write(&measure_tls_ssl, (const unsigned char *) text, len);
        if (sent < 0) {
            return sent;
        }
        text += sent;
        len -= (size_t) sent;
    }
    return 0;
}


// Bytes read, 0 at the end of the stream, below 0 on an error
static int measure_tls_read(measure_tls_conn_t *conn, uint8_t *buf, size_t size) {
    if (!conn->tls) {
        return lwip_recv(conn->fd, buf, size, 0);
    }
    int got = mbedtls_ssl_read(&measure_tls_ssl, buf, size);
    return got == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ? 0 : got;
}


static int measure_tls_getc(measure_tls_conn_t *conn) {
    if (measure_tls_rx_pos == measure_tls_rx_len) {
        int got = measure_tls_read(conn, measure_tls_rx, sizeof(measure_tls_rx));
        if (got <= 0) {
            return -1;
        }
        measure_tls_rx_len = (size_t) got;
        measure_tls_rx_pos = 0;
    }
    return measure_tls_rx[measure_tls_rx_pos++];
}


// One header line, its CR LF dropped; a line longer than the buffer is cut. False when the stream ends first
static bool measure_tls_read_line(measure_tls_conn_t *conn, char *line, size_t size) {
    size_t len = 0;
    while (true) {
        int c = measure_tls_getc(conn);
        if (c < 0) {
            return false;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r' && len + 1 < size) {
            line[len++] = (char) c;
        }
    }
    line[len] = '\0';
    return true;
}


static void measure_tls_fail(const char *what, int code) {
    snprintf(measure_tls_error, sizeof(measure_tls_error), "%s (-0x%04x)", what, (unsigned) -code);
}


static int measure_tls_connect(const measure_tls_url_t *url) {
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
    struct addrinfo *found = NULL;
    if (lwip_getaddrinfo(url->host, url->port, &hints, &found) != 0 || found == NULL) {
        measure_tls_fail("no address for the host", 0);
        return -1;
    }
    int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || lwip_connect(fd, found->ai_addr, found->ai_addrlen) != 0) {
        lwip_freeaddrinfo(found);
        if (fd >= 0) {
            lwip_close(fd);
        }
        measure_tls_fail("no connection", 0);
        return -1;
    }
    lwip_freeaddrinfo(found);
    struct timeval timeout = {.tv_sec = MEASURE_TLS_TIMEOUT_S};
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}


static bool measure_tls_handshake(measure_tls_conn_t *conn, const measure_tls_url_t *url, int hop) {
    mbedtls_ssl_init(&measure_tls_ssl);
    int ret = mbedtls_ssl_setup(&measure_tls_ssl, &measure_tls_config);
    if (ret == 0) {
        ret = mbedtls_ssl_set_hostname(&measure_tls_ssl, url->host);
    }
    if (ret != 0) {
        measure_tls_fail("no room for the session", ret);
        return false;
    }
    mbedtls_ssl_set_bio(&measure_tls_ssl, &conn->fd, measure_tls_send, measure_tls_recv, NULL);

    uint64_t started = time_us_64();
    while ((ret = mbedtls_ssl_handshake(&measure_tls_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            uint32_t flags = mbedtls_ssl_get_verify_result(&measure_tls_ssl);
            snprintf(measure_tls_error, sizeof(measure_tls_error), "handshake (-0x%04x), verify 0x%lx",
                     (unsigned) -ret, (unsigned long) flags);
            return false;
        }
    }
    measure_tls_handshake_ms[hop] = (uint32_t) ((time_us_64() - started) / 1000);
    return true;
}


static void measure_tls_close(measure_tls_conn_t *conn) {
    if (conn->tls) {
        mbedtls_ssl_close_notify(&measure_tls_ssl);
        mbedtls_ssl_free(&measure_tls_ssl);
    }
    lwip_close(conn->fd);
}


/*
    One request: its status, and either the body counted or the next URL taken from a redirect into `next`. False on
    a failure, its reason in measure_tls_error
*/
static bool measure_tls_hop(const char *text, int hop, char *next, bool *redirected) {
    measure_tls_url_t url;
    if (!measure_tls_parse_url(text, &url)) {
        measure_tls_fail("not an http or https URL", 0);
        return false;
    }
    measure_tls_conn_t conn = {.fd = measure_tls_connect(&url), .tls = url.tls};
    if (conn.fd < 0) {
        return false;
    }
    if (url.tls && !measure_tls_handshake(&conn, &url, hop)) {
        lwip_close(conn.fd);
        mbedtls_ssl_free(&measure_tls_ssl);
        return false;
    }

    // The port is named in Host only when it is not the scheme's own
    bool own_port = strcmp(url.port, url.tls ? "443" : "80") == 0;
    snprintf(measure_tls_request, sizeof(measure_tls_request), "GET %s HTTP/1.1\r\nHost: %s%s%s\r\n"
             "User-Agent: OpenTrickler-measure\r\nConnection: close\r\n\r\n", url.path, url.host, own_port ? "" : ":",
             own_port ? "" : url.port);
    if (measure_tls_write(&conn, measure_tls_request) != 0) {
        measure_tls_fail("the request was not sent", 0);
        measure_tls_close(&conn);
        return false;
    }

    measure_tls_rx_len = 0;
    measure_tls_rx_pos = 0;
    char *line = measure_tls_line;
    size_t line_size = sizeof(measure_tls_line);
    // The status after the version ("HTTP/1.1 302 Found"). Not sscanf: newlib's brings 30 KB of wide-character tables
    const char *status = NULL;
    if (!measure_tls_read_line(&conn, line, line_size) || strncmp(line, "HTTP/", 5) != 0 ||
        (status = strchr(line, ' ')) == NULL || (measure_tls_status = (int) strtol(status, NULL, 10)) <= 0) {
        measure_tls_fail("no status line", 0);
        measure_tls_close(&conn);
        return false;
    }
    long length = -1;
    *redirected = false;
    while (measure_tls_read_line(&conn, line, line_size) && line[0] != '\0') {
        if (strncasecmp(line, "content-length:", 15) == 0) {
            length = strtol(line + 15, NULL, 10);
        }
        else if (strncasecmp(line, "location:", 9) == 0 && measure_tls_status / 100 == 3) {
            const char *value = line + 9;
            while (*value == ' ') {
                value += 1;
            }
            snprintf(next, MEASURE_TLS_URL_MAX, "%s", value);
            *redirected = true;
        }
    }

    if (!*redirected) {
        // The rest of what was read with the headers, then the stream, counted up to its length or its end
        measure_tls_bytes = (uint32_t) (measure_tls_rx_len - measure_tls_rx_pos);
        while (length < 0 || (long) measure_tls_bytes < length) {
            int got = measure_tls_read(&conn, measure_tls_rx, sizeof(measure_tls_rx));
            if (got <= 0) {
                break;
            }
            measure_tls_bytes += (uint32_t) got;
        }
    }
    measure_tls_close(&conn);
    return true;
}


static bool measure_tls_setup(void) {
    mbedtls_platform_set_calloc_free(measure_tls_calloc, measure_tls_free);
    mbedtls_x509_crt_init(&measure_tls_ca);
    mbedtls_ssl_config_init(&measure_tls_config);
    mbedtls_entropy_init(&measure_tls_entropy);
    mbedtls_ctr_drbg_init(&measure_tls_drbg);

    static const unsigned char personal[] = "opentrickler";
    int ret = mbedtls_ctr_drbg_seed(&measure_tls_drbg, mbedtls_entropy_func, &measure_tls_entropy, personal,
                                    sizeof(personal) - 1);
    if (ret != 0) {
        measure_tls_fail("no random numbers", ret);
        return false;
    }
    ret = mbedtls_x509_crt_parse(&measure_tls_ca, (const unsigned char *) measure_ca_pem, sizeof(measure_ca_pem));
    if (ret != 0) {
        measure_tls_fail("the roots did not parse", ret);
        return false;
    }
    ret = mbedtls_ssl_config_defaults(&measure_tls_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        measure_tls_fail("no TLS configuration", ret);
        return false;
    }
    mbedtls_ssl_conf_authmode(&measure_tls_config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&measure_tls_config, &measure_tls_ca, NULL);
    mbedtls_ssl_conf_rng(&measure_tls_config, mbedtls_ctr_drbg_random, &measure_tls_drbg);
    return true;
}


static void measure_tls_teardown(void) {
    mbedtls_ssl_config_free(&measure_tls_config);
    mbedtls_x509_crt_free(&measure_tls_ca);
    mbedtls_ctr_drbg_free(&measure_tls_drbg);
    mbedtls_entropy_free(&measure_tls_entropy);
}


static void measure_tls_task(void *p) {
    (void) p;
    uint64_t started = time_us_64();
    bool ok = measure_tls_setup();
    int hop = 0;
    while (ok) {
        bool redirected = false;
        ok = measure_tls_hop(measure_tls_urls[hop % 2], hop, measure_tls_urls[(hop + 1) % 2], &redirected);
        hop += 1;
        if (!ok || !redirected) {
            break;
        }
        if (hop == MEASURE_TLS_MAX_HOPS) {
            measure_tls_fail("too many redirects", 0);
            ok = false;
        }
    }
    measure_tls_teardown();

    measure_tls_hops = hop;
    measure_tls_total_ms = (uint32_t) ((time_us_64() - started) / 1000);
    measure_tls_heap_min_after = xPortGetMinimumEverFreeHeapSize();
    measure_tls_stack_left = (uint32_t) (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    measure_tls_state = ok ? MEASURE_TLS_DONE : MEASURE_TLS_FAILED;
    printf("Measure: tls %s, status %d, %lu bytes, %d hops, %lu ms, mbedTLS peak %u bytes, %s\n",
           measure_tls_state_names[measure_tls_state], measure_tls_status, (unsigned long) measure_tls_bytes, hop,
           (unsigned long) measure_tls_total_ms, (unsigned) measure_tls_peak, ok ? "ok" : measure_tls_error);
    vTaskDelete(NULL);
}


static bool measure_tls_start(const char *url, char *reply, size_t reply_size) {
    if (measure_tls_state == MEASURE_TLS_RUNNING) {
        snprintf(reply, reply_size, "@err busy tls: a fetch is running");
        return false;
    }
    if (strlen(url) >= MEASURE_TLS_URL_MAX) {
        snprintf(reply, reply_size, "@err usage tls get <url>: the URL is too long");
        return false;
    }
    snprintf(measure_tls_urls[0], sizeof(measure_tls_urls[0]), "%s", url);
    measure_tls_status = 0;
    measure_tls_bytes = 0;
    memset(measure_tls_handshake_ms, 0, sizeof(measure_tls_handshake_ms));
    measure_tls_error[0] = '\0';
    measure_tls_peak = measure_tls_in_use;
    measure_tls_allocs = 0;
    measure_tls_alloc_failed = 0;
    measure_tls_heap_before = xPortGetFreeHeapSize();
    measure_tls_heap_min_before = xPortGetMinimumEverFreeHeapSize();
    measure_tls_state = MEASURE_TLS_RUNNING;
    if (xTaskCreate(measure_tls_task, "TLS", MEASURE_TLS_TASK_STACK_WORDS, NULL, MEASURE_TLS_TASK_PRIORITY,
                    NULL) != pdPASS) {
        measure_tls_state = MEASURE_TLS_FAILED;
        snprintf(reply, reply_size, "@err failed tls: no room for its task");
        return false;
    }
    snprintf(reply, reply_size, "@ok tls started");
    return true;
}


bool measure_tls_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                         void *write_ctx) {
    (void) write;
    (void) write_ctx;
    if (strncmp(args, "get ", 4) == 0) {
        return measure_tls_start(args + 4, reply, reply_size);
    }
    if (args[0] != '\0') {
        snprintf(reply, reply_size, "@err usage tls [get <url>]");
        return false;
    }
    snprintf(reply, reply_size, "@ok tls state=%s status=%d bytes=%lu hops=%d handshake_ms=%lu,%lu,%lu total_ms=%lu "
             "mbedtls_peak=%u mbedtls_now=%u allocs=%lu alloc_failed=%lu heap_before=%u heap_min_before=%u "
             "heap_min_after=%u stack_left=%lu error=\"%s\"",
             measure_tls_state_names[measure_tls_state], measure_tls_status, (unsigned long) measure_tls_bytes,
             measure_tls_hops, (unsigned long) measure_tls_handshake_ms[0], (unsigned long) measure_tls_handshake_ms[1],
             (unsigned long) measure_tls_handshake_ms[2], (unsigned long) measure_tls_total_ms,
             (unsigned) measure_tls_peak, (unsigned) measure_tls_in_use, (unsigned long) measure_tls_allocs,
             (unsigned long) measure_tls_alloc_failed, (unsigned) measure_tls_heap_before,
             (unsigned) measure_tls_heap_min_before, (unsigned) measure_tls_heap_min_after,
             (unsigned long) measure_tls_stack_left, measure_tls_error);
    return true;
}
