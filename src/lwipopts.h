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
// A socket's send copies what it sends into lwIP's heap, up to its send buffer, so the heap holds every connection's
// send buffer at once. At the examples' 8 segments (11.7 KB) one page load took the heap to 14,888 of its 16,384
// bytes; at 4, a soak with a tuning session's load ran it out 340 times and missed 43% of the web's reads. At 2
// segments (2.9 KB, lwIP's least that runs smoothly) the server's six connections hold at most about 19 KB together.
// TCP_SND_QUEUELEN follows it (common header)
#undef TCP_SND_BUF
#define TCP_SND_BUF (2 * TCP_MSS)

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

// lwIP's heap (ADR-003). Gate 1, on lwIP's file server: the Pico examples' 4,000 bytes ran out under two browsers and
// a charge (7,404 failed allocations, 8.9% of the web's reads missed); 16,384, the most two soaks used and half as much
// again, ran out none. Gate 2, on sockets: every byte sent is copied into this heap, up to each connection's send
// buffer, so it is sized by that, not by a soak: six connections of 2 segments with their headers, about 19 KB, and
// lwIP's own (the stack's state at rest is 1 KB; ARP, DHCP, mDNS and the ACKs it sends)
#undef MEM_SIZE
#define MEM_SIZE 24576

#endif