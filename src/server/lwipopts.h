#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#define LWIP_IGMP                   1  // ✅ Enable IGMP for IPv4 multicast
#define LWIP_NETIF_MULTICAST         1  // ✅ Ensure netif multicast filtering is available
#define LWIP_MULTICAST_PING          1  // ✅ Allow multicast ping

#define LWIP_IPV6                    1
#define LWIP_IPV6_NUM_ADDRESSES      4
#define LWIP_IPV6_REASS              1
#define LWIP_IPV6_MLD                1
#define LWIP_IPV6_DHCP               1
#define LWIP_IPV6_AUTOCONFIG         1
#define LWIP_IPV6_SCOPESID           1
#define LWIP_IPV6_NAME_RESOLUTION    1
#define LWIP_IPV6_FRAG               1
#define LWIP_IPV6_NDP                1

// allow override in some examples
#ifndef NO_SYS
#define NO_SYS                      1
#endif
// allow override in some examples
#ifndef LWIP_SOCKET
#define LWIP_SOCKET                 0
#endif
#if PICO_CYW43_ARCH_POLL
#define MEM_LIBC_MALLOC             1
#else
// MEM_LIBC_MALLOC is incompatible with non-polling versions
#define MEM_LIBC_MALLOC             0
#endif
#define MEM_ALIGNMENT               4
#ifndef MEM_SIZE
#define MEM_SIZE                    (48 * 1024)  // Increased from 4000 to 16000 for better buffer space
#endif
#define MEMP_NUM_TCP_SEG            32     // Increased from 32 to 64 to handle more concurrent TCP segments
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              48     // Increased from 24 to 32 to allow for more packet buffering
#define PBUF_POOL_BUFSIZE           1600   // Ensures packets can hold full TCP segments

#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1

// *** TCP Configuration Enhancements ***
#define TCP_WND                     (32 * TCP_MSS)  // Increased from 8 * TCP_MSS to 16 * TCP_MSS
#define TCP_MSS                     1460             // Maximum Transmission Unit (Ethernet standard)
#define TCP_SND_BUF                 (8 * TCP_MSS)    // Increased send buffer to 8 * TCP_MSS
#define TCP_SND_QUEUELEN            16               // Increased queue length for pending TCP segments

// *** Flow Control & Performance ***
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETCONN                0
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0
// #define ETH_PAD_SIZE                2
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// *** Additional TCP Optimizations ***
#define TCP_RECVMBOX_SIZE           64     // Ensure TCP has enough room for incoming packets
#define TCP_LISTEN_BACKLOG          1      // Enable backlog handling
#define TCP_DEFAULT_LISTEN_BACKLOG  8      // Allow up to 8 simultaneous connections
#define TCP_OVERSIZE                TCP_MSS // Allow full packet allocations

// *** Reduce Latency for Small Packets ***
#define LWIP_TCP_NODELAY            1      // Disable Nagle's algorithm for low-latency sending
#define TCP_QUEUE_OOSEQ             1      // Allow out-of-order packet queuing

#ifndef NDEBUG
#define LWIP_DEBUG                  1
#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          1
#endif

#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define PPP_DEBUG                   LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF

#endif /* __LWIPOPTS_H__ */