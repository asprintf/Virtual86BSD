/* socketvar.h - sockets (kern/uipc_socket.c). */
#ifndef _SYS_SOCKETVAR_H_
#define _SYS_SOCKETVAR_H_

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <netinet/in_var.h>

enum { SS_UNCONNECTED, SS_LISTENING, SS_CONNECTED, SS_DISCONNECTED };

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
  bool la_by_connect;    /* a datagram socket's address is the one connect()
                            chose (la_unbound before it) */
  struct inaddr la_unbound;
  bool conn_failed;      /* a connect() that did not come about */
  int raw_ttl;
};

/* uipc_socket.c, for the network */
struct sock *sock_new(int type);
void sock_put(struct sock *s);
void sock_wake(struct sock *s);
bool sock_queue(struct sock *s, const void *data, size_t n); /* stream data in */
void connect_failed(struct sock *s);
void sock_add_backlog(struct sock *listener, struct sock *c);
bool sock_backlog_full(struct sock *listener);

#endif
