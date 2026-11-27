/*
 * The web server's command on the remote control's console (ADR-003): `net`, lwIP's heap and the pools a web
 * connection takes from, the event streams, and `net log on|off`. Handed to the console at start-up
 * (http_server_console_init()), so the console names no web server (ADR-006, gate 6).
 */
#include <stdio.h>
#include <string.h>

#include "lwip/stats.h"
#include "lwip/memp.h"

#include "http_server.h"
#include "remote_control.h"


// The pools `net` reports: those every web connection takes from
static const struct {
    const char *name;
    memp_t pool;
} remote_control_net_pools[] = {
    {"tcp_pcb", MEMP_TCP_PCB},
    {"tcp_seg", MEMP_TCP_SEG},
    {"pbuf", MEMP_PBUF},
    {"pbuf_pool", MEMP_PBUF_POOL},
};


static bool remote_control_net(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                               void *write_ctx) {
    // `net log on|off`: each connection and request on the console, never stored
    if (args != NULL && strncmp(args, "log", 3) == 0) {
        const char *what = args + 3;
        while (*what == ' ') {
            what += 1;
        }
        bool on = strcmp(what, "on") == 0;
        if (!on && strcmp(what, "off") != 0) {
            snprintf(reply, reply_size, "@err usage net [log on|off]");
            return false;
        }
        http_server_log_requests(on);
        snprintf(reply, reply_size, "@ok net log %s", on ? "on" : "off");
        return true;
    }
    // lwIP's heap and pools (ADR-003, gate 1): in use now, the most ever in use, the size, and the allocations
    // that failed. Read without lwIP's lock: counters, only read here
    size_t len = snprintf(reply, reply_size, "@ok net heap_used=%u heap_max=%u heap_size=%u heap_err=%lu",
                          (unsigned) lwip_stats.mem.used,
                          (unsigned) lwip_stats.mem.max,
                          (unsigned) lwip_stats.mem.avail,
                          (unsigned long) lwip_stats.mem.err);
    for (size_t idx = 0; idx < sizeof(remote_control_net_pools) / sizeof(remote_control_net_pools[0]); idx += 1) {
        const struct stats_mem *pool = lwip_stats.memp[remote_control_net_pools[idx].pool];
        const char *name = remote_control_net_pools[idx].name;
        if (len >= reply_size) {
            break;
        }
        len += snprintf(reply + len, reply_size - len, " %s_used=%u %s_max=%u %s_size=%u %s_err=%lu",
                        name, (unsigned) pool->used,
                        name, (unsigned) pool->max,
                        name, (unsigned) pool->avail,
                        name, (unsigned long) pool->err);
    }
    // The event streams (ADR-008, gate 2): open now, opened and refused since the start
    http_server_stats_t server;
    http_server_get_stats(&server);
    if (len < reply_size) {
        snprintf(reply + len, reply_size - len, " live=%u live_opened=%lu live_refused=%lu", server.slots_live,
                 (unsigned long) server.live_opened, (unsigned long) server.live_refused);
    }
    return true;
}


bool http_server_console_init(void) {
    return remote_control_register("net", remote_control_net);
}
