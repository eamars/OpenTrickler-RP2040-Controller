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

// Lwip features
#define LWIP_HTTPD_CGI                  1    // Enable HTTPCGI
#define LWIP_HTTPD_SUPPORT_POST         0    // Enable POST
#define LWIP_HTTPD_CUSTOM_FILES         0   
#define LWIP_HTTPD_DYNAMIC_FILE_READ    0
#define LWIP_HTTPD_DYNAMIC_HEADERS      0
#define LWIP_HTTPD_MAX_REQUEST_URI_LEN  128
// lwIP takes 16 values from a request and drops the rest. The charge-mode page sends 17, and its last is "save to
// EEPROM" (ADR-012; audit H2). No more than the pages need: each value is 8 bytes in every connection's state, which
// comes out of lwIP's 4,000-byte heap. At 32 a third of the web's reads were lost under two browsers (ADR-012's
// progress, step 5)
#define LWIP_HTTPD_MAX_CGI_PARAMETERS   20
#define LWIP_HTTPD_DYNAMIC_HEADERS      0
#define LWIP_SOCKETS 1

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