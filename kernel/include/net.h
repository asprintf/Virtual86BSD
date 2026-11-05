/* net.h - sockets and the network: what fs/socket.c, net/ip.c, net/tcp.c
   and the drivers share. */
#ifndef NET_H
#define NET_H

#include "kernel.h"
#include "proc.h"

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

enum { SS_UNCONNECTED, SS_LISTENING, SS_CONNECTED, SS_DISCONNECTED };

/* an internet address: IPv6, IPv4 mapped; the port in host order */
struct inaddr {
  uint8_t a[16];
  uint16_t port;
};

struct ucred {
  int32_t pid;
  uint32_t uid, gid;
};

struct skb {
  struct list link;
  size_t len, off;
  struct file **fds;
  int nfds;
  struct ucred cred;
  char from[SUN_PATH + 2]; /* the sender's address (datagrams) */
  int fromlen;
  bool eor;
  char data[];
};

struct sock {
  int ref;
  int type;
  int state;
  struct sock *peer;
  struct list rq;        /* struct skb */
  size_t rbytes;
  int rcount;
  struct list backlog;   /* listening: connected socks not yet accepted */
  struct list blink;
  int nbacklog, maxbacklog;
  struct sock *listener; /* in a backlog: whose */
  char addr[SUN_PATH + 2];
  int addrlen;           /* 0: unbound */
  struct inode *bound;   /* file system name */
  bool shut_rd, shut_wr;
  bool passcred;
  bool netlink;
  int error;
  struct ucred peercred;
  struct waitq wq;       /* readers, writers, acceptors, connectors */
  struct file *file;
  uint64_t rcvtimeo, sndtimeo; /* ns, 0 = none */
  /* internet sockets */
  int family;            /* AF_UNIX, AF_INET, AF_INET6 */
  int proto;             /* IPPROTO_TCP, IPPROTO_UDP; 0 */
  struct inaddr la, ra;  /* local; remote (if has_ra) */
  bool in_bound, has_ra;
  bool reuse, v6only, nodelay, keepalive, broadcast;
  bool raw, hdrincl;     /* raw socket (type SOCK_DGRAM inside) */
  int32_t linger[2];
  struct list ilink;     /* in the table of bound internet sockets */
  struct tcb *tcb;       /* TCP over the wire (net/tcp.c) */
  bool autobound;        /* connect() chose the port */
  bool conn_failed;      /* a connect() that did not come about */
  int raw_ttl;
};

/* ---- shared by the socket layer and the network ---- */

struct tcb;
static inline bool in_v4(const struct inaddr *x) {
  static const uint8_t pre[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
  return !memcmp(x->a, pre, 12);
}
static inline uint32_t in_ip4(const struct inaddr *x) { /* host order */
  return (uint32_t)x->a[12] << 24 | (uint32_t)x->a[13] << 16 | (uint32_t)x->a[14] << 8 | x->a[15];
}
static inline void in_set4(struct inaddr *x, uint32_t ip, uint16_t port) {
  memset(x->a, 0, 10);
  x->a[10] = x->a[11] = 0xff;
  x->a[12] = (uint8_t)(ip >> 24), x->a[13] = (uint8_t)(ip >> 16), x->a[14] = (uint8_t)(ip >> 8), x->a[15] = (uint8_t)ip;
  x->port = port;
}

/* socket.c, for the network */
struct sock *sock_new(int type);
void sock_put(struct sock *s);
void sock_wake(struct sock *s);
struct sock *in_find(int proto, const struct inaddr *dst, const struct inaddr *src);
int in_put(int fam, const struct inaddr *x, char *out);
void udp_deliver(struct sock *t, const struct inaddr *src, const uint8_t *data, size_t n);
void raw_deliver(int proto, const struct inaddr *src, const struct inaddr *dst, const uint8_t *pkt, size_t n);
void udp_icmp_error(const struct inaddr *src, const struct inaddr *dst, int err);
bool sock_queue(struct sock *s, const void *data, size_t n); /* stream data in */
void connect_failed(struct sock *s);
void sock_add_backlog(struct sock *listener, struct sock *c);
bool sock_backlog_full(struct sock *listener);

/* the interface (net/ip.c) */
struct netif {
  bool present, configured;
  uint8_t mac[6];
  uint32_t addr, mask, gw, dns[3]; /* host order */
  int ndns;
  char domain[64];
  bool dhcp;
  uint64_t rx_packets, rx_bytes, tx_packets, tx_bytes, rx_dropped, tx_dropped;
  int (*xmit)(const void *frame, size_t len);
};
extern struct netif eth0;
void net_init_conf(const char *ipconf, const char *loss); /* ip=, netloss= */
void net_pnp(void (*out)(void *, const char *, ...), void *sb);
void net_rx(const void *frame, size_t len);  /* a driver: a frame came (from the network thread) */
void net_wake(void);                         /* (any context) */
bool ip_is_local(uint32_t ip);               /* one of this system's addresses */
bool ip_routable(uint32_t ip);               /* reachable through eth0 */
int ip_output(int proto, uint32_t src, uint32_t dst, const void *payload, size_t len);
int ip_output_raw(void *pkt, size_t len);    /* a whole IPv4 packet */
uint16_t ip_csum(const void *p, size_t n, uint32_t sum);
uint32_t ip_pseudo(uint32_t src, uint32_t dst, int proto, size_t len);
uint64_t net_now_ms(void);

/* TCP over the wire (net/tcp.c) */
int tcp_connect(struct sock *s, const struct inaddr *src, const struct inaddr *dst, bool nonblock);
int tcp_connect_wait(struct sock *s, bool nonblock);
long tcp_send(struct sock *s, const char *ubuf, size_t n, bool nonblock, int flags);
bool tcp_writable(struct sock *s);
void tcp_shutdown_wr(struct sock *s);
void tcp_close(struct sock *s);       /* the socket goes away */
void tcp_listener_close(struct sock *s);
void tcp_read_done(struct sock *s);   /* the reader made room */
void tcp_input(uint32_t src, uint32_t dst, const uint8_t *seg, size_t len);
uint64_t tcp_timers(uint64_t now_ms);  /* the next deadline, or 0 */
void tcp_icmp_error(uint32_t src, uint32_t dst, const uint8_t *hdr, size_t n, int err);

#endif
