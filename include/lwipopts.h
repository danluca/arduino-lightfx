// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

// lwIP options as seen by the PROJECT sources.
//
// !!! The framework's include/lwipopts.h is the MASTER !!!
//     (<platformio packages>/framework-arduinopico/include/lwipopts.h)
// The lwIP stack itself (memory heap and pools, pbuf pool, TCP/UDP control blocks, TCP window and buffers, DHCP, DNS, SNTP and
// the other lwIP sources) is NOT compiled from this project. It comes precompiled in the framework library
//     <platformio packages>/framework-arduinopico/lib/rp2350/liblwip.a
// built with the framework's lwipopts.h. Editing options here does NOT change the sizes or the behavior of the compiled stack - which means
// these cannot be changed from the project (they need a liblwip.a rebuild, tools/libpico/make-libpico.sh in the framework fork):
//   - memory sizes: MEM_SIZE (16 KB lwIP heap), PBUF_POOL_SIZE (24), MEMP_NUM_TCP_PCB (5), MEMP_NUM_UDP_PCB (7), MEMP_NUM_TCP_SEG, MEMP_NUM_ARP_QUEUE
//   - TCP tuning: TCP_MSS, TCP_WND, TCP_SND_BUF, TCP_SND_QUEUELEN, TCP_DEFAULT_LISTEN_BACKLOG
//   - SNTP: SNTP_MAX_SERVERS (2), SNTP_SET_SYSTEM_TIME_US, SNTP_SERVER_DNS; the poll interval SNTP_UPDATE_DELAY and the time validation are
//     handled with build flags in platformio.ini instead (see docs/okf/scheduling/time-sync.md)
//   - checksum algorithm, statistics, debug switches, locking model and so on
//
// What remains below are only the options that the project sources can see through the lwIP headers (included via the framework WiFi,
// LEAmDNS and HTTPClient libraries): the platform hooks and the feature switches that shape the lwIP structures (netif, pcb, dhcp, ...).
// The values MUST stay identical to the framework file - a difference would compile the project against different structure layouts than
// the ones inside liblwip.a. If the framework file changes (framework upgrade, fork update), mirror the change here.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Critical section protection
extern void noInterrupts();
extern void interrupts();
#define SYS_ARCH_DECL_PROTECT int
#define SYS_ARCH_PROTECT(lev) {(void) lev; noInterrupts();}
#define SYS_ARCH_UNPROTECT(lev) {(void) lev; interrupts();}

#ifndef DEBUG_RP2040_PORT
extern void panic(const char *fmt, ...);
#define LWIP_PLATFORM_ASSERT(x) panic("lwip")
#endif

extern void __lwip_assert_core_locked();
#define LWIP_ASSERT_CORE_LOCKED() {__lwip_assert_core_locked();}

extern unsigned long __lwip_rand(void);
#define LWIP_RAND() __lwip_rand()

// Stack model and protocol/feature switches
#define NO_SYS                        1
#define LWIP_SOCKET                   0
#define LWIP_NETCONN                  0
#define MEM_LIBC_MALLOC               0
#define MEM_ALIGNMENT                 4
#define LWIP_ARP                      7
#define LWIP_ETHERNET                 1
#define LWIP_ICMP                     1
#define LWIP_RAW                      1
#define LWIP_IPV4                     1
#define LWIP_TCP                      1
#define LWIP_UDP                      1
#define LWIP_DHCP                     1
#define LWIP_DNS                      1
#define LWIP_DNS_SUPPORT_MDNS_QUERIES 1
#define LWIP_IGMP                     1
#define LWIP_MDNS_RESPONDER           1
#define MDNS_MAX_SERVICES             4
#define LWIP_TCP_KEEPALIVE            1
#define TCP_LISTEN_BACKLOG            1
#define DHCP_DOES_ARP_CHECK           0
#define LWIP_DHCP_DOES_ACD_CHECK      0

// Network interface features
#define LWIP_NETIF_STATUS_CALLBACK    1
#define LWIP_NETIF_LINK_CALLBACK      1
#define LWIP_NETIF_HOSTNAME           1
#define LWIP_NUM_NETIF_CLIENT_DATA    5
#define LWIP_NETIF_TX_SINGLE_PBUF     1

// Statistics are not compiled into liblwip.a
#define LWIP_STATS                    0
#define LWIP_STATS_DISPLAY            0
#define MEM_STATS                     0
#define SYS_STATS                     0
#define MEMP_STATS                    0
#define LINK_STATS                    0

#ifdef __cplusplus
}
#endif // __cplusplus
