#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Generally you would define your own explicit list of lwIP options
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html)
//
// This example uses a common include to avoid repetition
#include "lwipopts_examples_common.h"



#if !NO_SYS
#define TCPIP_THREAD_STACKSIZE 4096
#define DEFAULT_THREAD_STACKSIZE 1024
#define ASYNC_CONTEXT_DEFAULT_FREERTOS_TASK_STACK_SIZE 4096
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_MBOX_SIZE 8
#define LWIP_TIMEVAL_PRIVATE 0
#define TCPIP_THREAD_PRIO   7

// // not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1

// // ping_thread sets socket receive timeout, so enable this feature
#define LWIP_SO_RCVTIMEO 1
#endif

// lwIP's sockets, for our own server (ADR-003, gate 2). This said `LWIP_SOCKETS 1`, which is no lwIP option: the
// sockets and the netconn layer under them were off. lwIP's names only (lwip_socket, lwip_close...), so no other file
// gets macros named close, read or select
#undef LWIP_SOCKET
#define LWIP_SOCKET 1
#undef LWIP_NETCONN
#define LWIP_NETCONN 1
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
// A reset in place of a close for a reply that stalls, so lwIP frees what it held at once
#define LWIP_SO_LINGER 1
// The mailboxes a socket's data and its new connections wait in: lwIP's 0 would make none
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
// Six clients, the listener, one waiting to be taken, and room for one being closed
#define MEMP_NUM_NETCONN 10
// A socket's send copies what it sends into lwIP's heap, up to its send buffer. At the examples' 8 segments
// (11.7 KB) one page load took the heap to 14,888 of its 16,384 bytes, and two at once would run it out. 4 segments
// (5.8 KB): two page loads at once fit beside everything else. TCP_SND_QUEUELEN follows it (common header)
#undef TCP_SND_BUF
#define TCP_SND_BUF (4 * TCP_MSS)

// MDNS
#define LWIP_MDNS_RESPONDER 1
#define LWIP_IGMP 1
#define LWIP_NUM_NETIF_CLIENT_DATA 1
#define MDNS_MAX_SERVICES 2  // increase from 1 to 2 
#define MDNS_RESP_USENETIF_EXTCALLBACK  1
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 8)
#define MEMP_NUM_TCP_PCB 12

// lwIP's heap and pools counted for the console's `net` (ADR-003, gate 1), whether NDEBUG is defined or not. The
// counters are 32 bits: a soak's failed allocations can pass 65,535
#undef LWIP_STATS
#define LWIP_STATS 1
#define LWIP_STATS_LARGE 1
#undef MEM_STATS
#define MEM_STATS 1
#undef MEMP_STATS
#define MEMP_STATS 1

// lwIP's heap (ADR-003, gate 1). It holds each web connection's state, the header of each TCP segment in flight and
// a copy of each reply from RAM. The Pico examples' 4,000 bytes ran out under two browsers and a charge: 7,404 failed
// allocations and 8.9% of the web's reads missed in ten minutes. With room to spare the web used 8,824 and 10,364 at
// most in two soaks of the same load: the most moves with the Wi-Fi. This is the larger and half as much again
#undef MEM_SIZE
#define MEM_SIZE 16384

#endif