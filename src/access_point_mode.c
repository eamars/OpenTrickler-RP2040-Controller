#include <FreeRTOS.h>
#include <task.h>
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"


typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

TCP_SERVER_T state;


TaskHandle_t cyw43_poll_handler = NULL;


// static bool tcp_server_open(void *arg) {
//     TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
//     printf("starting server on port %u\n", TCP_PORT);

//     struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
//     if (!pcb) {
//         printf("failed to create pcb\n");
//         return false;
//     }

//     err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
//     if (err) {
//         printf("failed to bind to port %d\n");
//         return false;
//     }

//     state->server_pcb = tcp_listen_with_backlog(pcb, 1);
//     if (!state->server_pcb) {
//         printf("failed to listen\n");
//         if (pcb) {
//             tcp_close(pcb);
//         }
//         return false;
//     }

//     tcp_arg(state->server_pcb, state);
//     tcp_accept(state->server_pcb, tcp_server_accept);

//     return true;
// }


void access_point_mode_init() {
    const char ap_name[] = "OpenTricklerAP";
    cyw43_arch_enable_ap_mode(ap_name, "1234567890", CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&state.gw), 192, 168, 3, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start DHCP server
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state.gw, &mask);

    // Start the DNS server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &state.gw);

    // if (!tcp_server_open(state)) {
    //     printf("failed to open server\n");
    //     exit(-1);
    // }

}