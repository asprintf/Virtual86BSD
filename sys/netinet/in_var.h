/* in_var.h - internet addresses and the network layer (netinet/ip.c,
   netinet/tcp.c) as the socket layer sees them. */
#ifndef _NETINET_IN_VAR_H_
#define _NETINET_IN_VAR_H_

#include <sys/systm.h>
#include <sys/socket.h>

struct sock;
struct file;

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

struct sock *in_find(int proto, const struct inaddr *dst, const struct inaddr *src);
int in_put(int fam, const struct inaddr *x, char *out);
void udp_deliver(struct sock *t, const struct inaddr *src, const uint8_t *data, size_t n);
void raw_deliver(int proto, const struct inaddr *src, const struct inaddr *dst, const uint8_t *pkt, size_t n);
void udp_icmp_error(const struct inaddr *src, const struct inaddr *dst, int err);

/* TCP over the wire (netinet/tcp.c) */
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
