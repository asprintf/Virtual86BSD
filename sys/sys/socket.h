/* socket.h - socket constants (the ABI part, Linux values). */
#ifndef _SYS_SOCKET_H_
#define _SYS_SOCKET_H_

#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPV6_V6ONLY 26
#define TCP_NODELAY 1
#define IPPROTO_ICMP 1
#define IPPROTO_ICMPV6 58
#define IPPROTO_RAW 255
#define IP_HDRINCL 3
#define SO_REUSEPORT 15
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_SEQPACKET 5
#define AF_NETLINK 16
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC 02000000
#define SOCK_CLOFORK 010000000000
#define MSG_CMSG_CLOFORK 0x80000000U
#define SOL_SOCKET 1
#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_LINGER 13
#define SO_PASSCRED 16
#define SO_PEERCRED 17
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_ACCEPTCONN 30
#define SO_PROTOCOL 38
#define SO_DOMAIN 39
#define SCM_RIGHTS 1
#define SCM_CREDENTIALS 2
#define MSG_OOB 1
#define MSG_PEEK 2
#define MSG_DONTROUTE 4
#define MSG_CTRUNC 8
#define MSG_TRUNC 0x20
#define MSG_DONTWAIT 0x40
#define MSG_EOR 0x80
#define MSG_WAITALL 0x100
#define MSG_NOSIGNAL 0x4000
#define MSG_CMSG_CLOEXEC 0x40000000
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SOCK_BUF (256 * 1024)
#define MAX_DGRAMS 512
#define SUN_PATH 108

#endif
