/* socket.c - sockets: the AF_UNIX family (stream, datagram, seqpacket;
   file system and abstract names; SCM_RIGHTS and SCM_CREDENTIALS), and
   AF_INET/AF_INET6 over the loopback interface.

   Internet sockets: TCP (SOCK_STREAM) and UDP (SOCK_DGRAM) between the
   addresses of the one interface there is, lo: 127.0.0.0/8 and ::1 (the
   wildcard addresses bind to it, and connecting to them reaches it).  They
   use the same connected pairs and queues as AF_UNIX: on a loopback that
   is what TCP and UDP deliver, with nothing in between to lose, reorder
   or delay.  What is TCP/IP proper (packets, an IP layer, a network
   driver) comes with a network device.  Addresses are kept as IPv6
   addresses, IPv4 ones mapped (::ffff:a.b.c.d); an AF_INET6 socket is
   dual-stack unless IPV6_V6ONLY.  Other destinations: ENETUNREACH.

   Raw sockets (root only): what one sends to a loopback address reaches
   every raw socket of the protocol, IPv4 with its IP header (made here, or
   the sender's with IP_HDRINCL / IPPROTO_RAW); the kernel answers ICMP and
   ICMPv6 echo requests, and computes ICMPv6 checksums.  Inside, a raw
   socket is a datagram socket with `raw' set. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "syscall.h"

#include "net.h"

static struct super *sock_sb;
static const struct file_ops sock_fops;
static struct list abstract = LIST_INIT(abstract); /* abstract names: socks */
static struct list raw_socks = LIST_INIT(raw_socks); /* raw internet sockets */

struct absname {
  struct list link;
  struct sock *s;
};

struct sock *sock_new(int type) {
  struct sock *s = kzalloc(sizeof(*s));
  if (!s)
    return NULL;
  s->ref = 1;
  s->type = type;
  list_init(&s->rq);
  list_init(&s->backlog);
  list_init(&s->blink);
  list_init(&s->ilink);
  waitq_init(&s->wq);
  s->family = AF_UNIX;
  return s;
}

static bool is_inet(struct sock *s) { return s->family == AF_INET || s->family == AF_INET6; }

static void skb_free(struct skb *b) {
  for (int i = 0; i < b->nfds; i++)
    file_put(b->fds[i]);
  kfree(b->fds);
  kfree(b);
}

void sock_put(struct sock *s) {
  if (--s->ref > 0)
    return;
  list_for_each_safe(p, n, &s->rq) {
    list_del(p);
    skb_free(list_entry(p, struct skb, link));
  }
  kfree(s);
}

void sock_wake(struct sock *s) {
  wake_up(&s->wq);
  poll_wake();
}

static void unbind(struct sock *s) {
  if (s->in_bound || s->raw) {
    s->raw = false;
    list_del(&s->ilink);
    list_init(&s->ilink);
    s->in_bound = false;
  }
  if (s->bound) {
    s->bound->sock = NULL;
    iput(s->bound);
    s->bound = NULL;
  }
  list_for_each(p, &abstract) {
    struct absname *a = list_entry(p, struct absname, link);
    if (a->s == s) {
      list_del(&a->link);
      kfree(a);
      break;
    }
  }
}

static void sock_close(struct sock *s) {
  if (s->tcb)
    tcp_close(s);
  if (s->state == SS_LISTENING && is_inet(s))
    tcp_listener_close(s);
  unbind(s);
  /* pending connections of a listener are refused */
  list_for_each_safe(p, n, &s->backlog) {
    struct sock *c = list_entry(p, struct sock, blink);
    list_del(&c->blink);
    if (c->tcb) { /* through the network, never accepted: reset */
      c->linger[0] = 1, c->linger[1] = 0;
      tcp_close(c);
    }
    if (c->peer) {
      c->peer->peer = NULL;
      c->peer->state = SS_DISCONNECTED;
      c->peer->error = ECONNRESET;
      sock_wake(c->peer);
      sock_put(c->peer);
      c->peer = NULL;
    }
    sock_put(c);
  }
  if (s->peer) {
    struct sock *p = s->peer;
    if (s->type != SOCK_DGRAM) {
      p->peer = NULL;
      p->state = SS_DISCONNECTED;
      p->shut_rd = true; /* reads see end of file once the data is read */
      if (!list_empty(&s->rq))
        p->error = ECONNRESET;
      sock_wake(p);
      sock_put(s); /* the peer's reference to us */
    }
    sock_put(p);
    s->peer = NULL;
  }
  s->state = SS_DISCONNECTED;
  /* datagram sockets connected to us: their sends now fail */
  sock_wake(s);
}

static struct sock *SK(struct file *f) { return f->priv; }

static struct file *sock_file(struct sock *s, unsigned flags) {
  if (!sock_sb) {
    static struct fs_type st = {.name = "sockfs"};
    static const struct super_ops sops = {.write_inode = NULL};
    sock_sb = super_alloc(&st, NULL);
    sock_sb->ops = &sops;
    sock_sb->blocksize = PAGE_SIZE;
    sock_sb->magic = 0x534f434b;
    sock_sb->no_reopen = true;
  }
  struct inode *ino = new_inode(sock_sb, ++sock_sb->next_ino);
  if (!ino)
    return NULL;
  ino->mode = S_IFSOCK | 0777;
  ino->nlink = 1;
  ino->uid = cur_proc()->cred.fsuid;
  ino->gid = cur_proc()->cred.fsgid;
  ino->fops = &sock_fops;
  inode_set_times(ino, true, true, true);
  struct file *f = file_alloc(ino, NULL, O_RDWR | (flags & O_NONBLOCK));
  iput(ino);
  if (!f)
    return NULL;
  f->priv = s;
  s->file = f;
  char n[32];
  snprintf(n, sizeof(n), "socket:[%lu]", ino->ino);
  f->path = kstrdup(n);
  return f;
}

static int install(struct sock *s, int flags) {
  struct file *f = sock_file(s, (unsigned)flags & SOCK_NONBLOCK);
  if (!f) {
    sock_put(s);
    return -ENFILE;
  }
  int fd = fd_install(f, 0, fd_flags_of(flags));
  if (fd < 0)
    file_put(f);
  return fd;
}

static struct file *sock_file_of(int fd, int *err) {
  struct file *f = fd_get(fd);
  if (!f) {
    *err = -EBADF;
    return NULL;
  }
  if (f->fops != &sock_fops) {
    *err = -ENOTSOCK;
    return NULL;
  }
  return f;
}

static struct sock *sock_of(int fd, int *err) {
  struct file *f = fd_get(fd);
  if (!f) {
    *err = -EBADF;
    return NULL;
  }
  if (f->fops != &sock_fops) {
    *err = -ENOTSOCK;
    return NULL;
  }
  return SK(f);
}

long sys_socket(long domain, long type, long proto) {
  int t = (int)type & 0xf;
  if (type & ~(0xfL | SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_CLOFORK))
    return -EINVAL;
  if (domain == AF_NETLINK) {
    /* only as the kernel's notification channel (mq_notify SIGEV_THREAD) */
    if (t != SOCK_RAW && t != SOCK_DGRAM)
      return -ESOCKTNOSUPPORT;
    struct sock *s = sock_new(SOCK_DGRAM);
    if (!s)
      return -ENOMEM;
    s->netlink = true;
    s->state = SS_CONNECTED;
    return install(s, (int)type);
  }
  if (domain == AF_INET || domain == AF_INET6) {
    int pr;
    if (t == SOCK_RAW) {
      if (proto <= 0 || proto > 255)
        return -EPROTONOSUPPORT;
      if (!capable())
        return -EPERM;
      struct sock *s = sock_new(SOCK_DGRAM);
      if (!s)
        return -ENOMEM;
      s->family = (int)domain;
      s->proto = (int)proto;
      s->raw = true;
      s->hdrincl = proto == IPPROTO_RAW;
      list_add_tail(&raw_socks, &s->ilink);
      return install(s, (int)type);
    }
    if (t == SOCK_STREAM)
      pr = IPPROTO_TCP;
    else if (t == SOCK_DGRAM)
      pr = IPPROTO_UDP;
    else
      return t == SOCK_SEQPACKET ? -EPROTONOSUPPORT : -ESOCKTNOSUPPORT;
    if (proto != 0 && proto != pr)
      return -EPROTONOSUPPORT;
    struct sock *s = sock_new(t);
    if (!s)
      return -ENOMEM;
    s->family = (int)domain;
    s->proto = pr;
    return install(s, (int)type);
  }
  if (domain != AF_UNIX)
    return -EAFNOSUPPORT;
  if (t != SOCK_STREAM && t != SOCK_DGRAM && t != SOCK_SEQPACKET)
    return -ESOCKTNOSUPPORT;
  if (proto != 0)
    return -EPROTONOSUPPORT;
  struct sock *s = sock_new(t);
  if (!s)
    return -ENOMEM;
  return install(s, (int)type);
}

/* a message from the kernel to a netlink socket */
int netlink_deliver(struct file *f, const void *data, size_t n) {
  if (!f || f->fops != &sock_fops || !SK(f) || !SK(f)->netlink)
    return -EBADF;
  struct sock *s = SK(f);
  struct skb *b = kzalloc(sizeof(*b) + n + 1);
  if (!b)
    return -ENOMEM;
  b->len = n;
  memcpy(b->data, data, n);
  uint64_t fl = irq_save();
  list_add_tail(&s->rq, &b->link);
  s->rbytes += n;
  s->rcount++;
  irq_restore(fl);
  sock_wake(s);
  return 0;
}

bool is_netlink_file(struct file *f) { return f && f->fops == &sock_fops && SK(f) && SK(f)->netlink; }

static struct ucred my_cred(void) {
  struct ucred c = {cur_proc()->pid, cur_proc()->cred.euid, cur_proc()->cred.egid};
  return c;
}

long sys_socketpair(long domain, long type, long proto, long usv) {
  int t = (int)type & 0xf;
  if (domain != AF_UNIX)
    return -EAFNOSUPPORT;
  if (t != SOCK_STREAM && t != SOCK_DGRAM && t != SOCK_SEQPACKET)
    return -ESOCKTNOSUPPORT;
  if (proto != 0)
    return -EPROTONOSUPPORT;
  struct sock *a = sock_new(t), *b = sock_new(t);
  if (!a || !b) {
    kfree(a);
    kfree(b);
    return -ENOMEM;
  }
  a->peer = b;
  b->peer = a;
  a->ref++;
  b->ref++;
  a->state = b->state = SS_CONNECTED;
  a->peercred = b->peercred = my_cred();
  int fa = install(a, (int)type);
  if (fa < 0) {
    sock_close(b);
    sock_put(b);
    return fa;
  }
  int fb = install(b, (int)type);
  if (fb < 0) {
    fd_close(fa);
    return fb;
  }
  int32_t sv[2] = {fa, fb};
  if (copy_to_user((void *)usv, sv, 8)) {
    fd_close(fa);
    fd_close(fb);
    return -EFAULT;
  }
  return 0;
}

/* ---- addresses ---- */

static int get_addr(long uaddr, long len, char *out, int *outlen) {
  if (len < 2 || len > SUN_PATH + 2)
    return -EINVAL;
  if (copy_from_user(out, (void *)uaddr, (size_t)len))
    return -EFAULT;
  uint16_t fam;
  memcpy(&fam, out, 2);
  if (fam != AF_UNIX)
    return -EINVAL;
  *outlen = (int)len;
  if (len > 2 && out[2] != 0) {
    /* a path: up to the NUL */
    size_t n = strnlen(out + 2, (size_t)len - 2);
    out[2 + n] = 0;
    *outlen = (int)(2 + n + 1);
  }
  return 0;
}

static bool is_abstract(const char *a, int len) { return len > 2 && a[2] == 0; }

static struct sock *find_abstract(const char *a, int len) {
  list_for_each(p, &abstract) {
    struct absname *n = list_entry(p, struct absname, link);
    if (n->s->addrlen == len && !memcmp(n->s->addr, a, (size_t)len))
      return n->s;
  }
  return NULL;
}

/* the socket an address names (with a reference) */
static struct sock *lookup_addr(const char *a, int len, int *err) {
  if (len <= 2) {
    *err = -EINVAL;
    return NULL;
  }
  if (is_abstract(a, len)) {
    struct sock *s = find_abstract(a, len);
    if (!s) {
      *err = -ECONNREFUSED;
      return NULL;
    }
    s->ref++;
    return s;
  }
  struct path p;
  int e = namei_at(AT_FDCWD, a + 2, LOOKUP_FOLLOW, &p);
  if (e) {
    *err = e;
    return NULL;
  }
  e = permission(p.ino, MAY_WRITE);
  struct sock *s = p.ino->sock;
  if (!S_ISSOCK(p.ino->mode))
    e = -ECONNREFUSED;
  path_put(&p);
  if (e) {
    *err = e;
    return NULL;
  }
  if (!s) {
    *err = -ECONNREFUSED;
    return NULL;
  }
  s->ref++;
  return s;
}

/* ---- internet addresses ---- */

static struct list inet_bound = LIST_INIT(inet_bound); /* bound internet socks */
static uint16_t next_ephemeral = 32768;

static const uint8_t v4pre[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
static bool in_any(const struct inaddr *x) {
  static const uint8_t z[16];
  return in_v4(x) ? !memcmp(x->a + 12, z, 4) : !memcmp(x->a, z, 16);
}
static bool in_loop(const struct inaddr *x) {
  static const uint8_t l6[16] = {[15] = 1};
  return in_v4(x) ? x->a[12] == 127 : !memcmp(x->a, l6, 16);
}
static bool in_same(const struct inaddr *x, const struct inaddr *y) { return !memcmp(x->a, y->a, 16); }
/* the loopback address in x's family (for the wildcard as a destination) */
static void in_loopback(struct inaddr *x, bool v4) {
  memset(x->a, 0, 16);
  if (v4) {
    memcpy(x->a, v4pre, 12);
    x->a[12] = 127, x->a[15] = 1;
  } else
    x->a[15] = 1;
}

/* a sockaddr_in or sockaddr_in6 from the user, for socket s */
static int in_get(struct sock *s, long uaddr, long len, struct inaddr *out) {
  uint8_t b[28];
  uint16_t fam;
  if (len < 2)
    return -EINVAL;
  if (copy_from_user(b, (void *)uaddr, (size_t)MIN(len, 28L)))
    return -EFAULT;
  memcpy(&fam, b, 2);
  memset(out, 0, sizeof(*out));
  if (fam == AF_INET) {
    if (len < 16)
      return -EINVAL;
    if (s->family == AF_INET6 && s->v6only)
      return -EAFNOSUPPORT;
    memcpy(out->a, v4pre, 12);
    memcpy(out->a + 12, b + 4, 4);
  } else if (fam == AF_INET6 && s->family == AF_INET6) {
    if (len < 24)
      return -EINVAL;
    memcpy(out->a, b + 8, 16);
    if (s->v6only && in_v4(out))
      return -EAFNOSUPPORT;
  } else
    return s->family == AF_INET ? -EAFNOSUPPORT : -EINVAL;
  out->port = (uint16_t)(b[2] << 8 | b[3]);
  return 0;
}

/* render x for a socket of family fam: the length */
int in_put(int fam, const struct inaddr *x, char *out) {
  if (fam == AF_INET) {
    memset(out, 0, 16);
    out[0] = AF_INET;
    out[2] = (char)(x->port >> 8), out[3] = (char)x->port;
    memcpy(out + 4, x->a + 12, 4);
    return 16;
  }
  memset(out, 0, 28);
  out[0] = AF_INET6;
  out[2] = (char)(x->port >> 8), out[3] = (char)x->port;
  memcpy(out + 8, x->a, 16);
  return 28;
}

/* does a socket bound to w (v6only as given) take traffic for a? */
static bool in_covers(const struct inaddr *w, bool v6only, const struct inaddr *a) {
  if (in_same(w, a))
    return true;
  if (!in_any(w))
    return false;
  if (in_v4(w))
    return in_v4(a);
  return !v6only || !in_v4(a);
}

static bool in_conflict(struct sock *s, const struct inaddr *a) {
  list_for_each(p, &inet_bound) {
    struct sock *x = list_entry(p, struct sock, ilink);
    if (x == s || x->proto != s->proto || x->la.port != a->port)
      continue;
    if (!in_covers(&x->la, x->v6only, a) && !in_covers(a, s->v6only, &x->la))
      continue;
    if (s->reuse && x->reuse && (s->proto == IPPROTO_UDP || x->state != SS_LISTENING))
      continue;
    return true;
  }
  return false;
}

/* bind s to a (port 0: an ephemeral one) */
static int in_bind(struct sock *s, struct inaddr *a) {
  if (s->in_bound)
    return -EINVAL;
  if (!in_any(a) && !in_loop(a) && !(in_v4(a) && ip_is_local(in_ip4(a))))
    return -EADDRNOTAVAIL;
  if (s->raw) { /* (no ports; already on raw_socks) */
    s->la = *a;
    s->la.port = 0;
    s->in_bound = true;
    return 0;
  }
  if (a->port && a->port < 1024 && !capable())
    return -EACCES;
  if (!a->port) {
    for (int n = 0; n < 28232 && !a->port; n++) {
      a->port = next_ephemeral;
      next_ephemeral = next_ephemeral >= 60999 ? 32768 : next_ephemeral + 1;
      bool r = s->reuse;
      s->reuse = false;
      if (in_conflict(s, a))
        a->port = 0;
      s->reuse = r;
    }
    if (!a->port)
      return -EADDRINUSE;
  } else if (in_conflict(s, a))
    return -EADDRINUSE;
  s->la = *a;
  s->in_bound = true;
  list_add_tail(&inet_bound, &s->ilink);
  return 0;
}

/* an unbound socket gets the wildcard address and an ephemeral port */
static int in_autobind(struct sock *s) {
  if (s->in_bound)
    return 0;
  struct inaddr a;
  memset(&a, 0, sizeof(a));
  if (s->family == AF_INET)
    memcpy(a.a, v4pre, 12);
  return in_bind(s, &a);
}

/* a destination: the wildcard means the loopback; else it must be there */
/* 0: on this system (the loopback, or eth0's own address); 1: through
   the network interface (IPv4); or an error */
static int in_dest(struct sock *s, struct inaddr *a) {
  if (in_any(a))
    in_loopback(a, in_v4(a) || s->family == AF_INET);
  else if (in_loop(a) || (in_v4(a) && ip_is_local(in_ip4(a))))
    return 0;
  else if (in_v4(a) && ip_routable(in_ip4(a)))
    return 1;
  else
    return -ENETUNREACH;
  return 0;
}

/* the source address s sends from towards dst */
static void in_src(struct sock *s, const struct inaddr *dst, struct inaddr *src) {
  *src = s->la;
  if (in_any(src)) {
    uint16_t port = src->port;
    if (in_v4(dst) && !ip_is_local(in_ip4(dst)))
      in_set4(src, eth0.addr, port); /* through the network: eth0's */
    else if (in_v4(dst))
      *src = *dst; /* 127.x.y.z (or eth0's address) answers from itself */
    else
      in_loopback(src, false);
    src->port = port;
  }
}

/* the socket of type proto that takes traffic to dst from src (with a
   reference): the most specific address wins; a listener for TCP; for UDP
   a connected socket only from its peer */
struct sock *in_find(int proto, const struct inaddr *dst, const struct inaddr *src) {
  struct sock *best = NULL;
  int bestscore = -1;
  list_for_each(p, &inet_bound) {
    struct sock *x = list_entry(p, struct sock, ilink);
    if (x->proto != proto || x->la.port != dst->port || !in_covers(&x->la, x->v6only, dst))
      continue;
    if (proto == IPPROTO_TCP && x->state != SS_LISTENING)
      continue;
    int score = in_same(&x->la, dst) ? 2 : 0;
    if (proto == IPPROTO_UDP && x->has_ra) {
      if (!in_same(&x->ra, src) || x->ra.port != src->port)
        continue;
      score++;
    }
    if (score >= bestscore) /* the last bound of equals (SO_REUSEADDR) */
      best = x, bestscore = score;
  }
  if (best)
    best->ref++;
  return best;
}

long sys_bind(long fd, long uaddr, long len) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  if (s->netlink)
    return 0;
  if (is_inet(s)) {
    struct inaddr ia;
    if ((e = in_get(s, uaddr, len, &ia)))
      return e;
    return in_bind(s, &ia);
  }
  char a[SUN_PATH + 2];
  int alen;
  if ((e = get_addr(uaddr, len, a, &alen)))
    return e;
  if (s->addrlen)
    return -EINVAL;
  if (alen == 2)
    return -EINVAL; /* autobind is not supported */
  if (is_abstract(a, alen)) {
    if (find_abstract(a, alen))
      return -EADDRINUSE;
    struct absname *n = kmalloc(sizeof(*n));
    n->s = s;
    list_add_tail(&abstract, &n->link);
  } else {
    struct path dir, ex;
    const char *last;
    size_t ll;
    e = nameiparent_at(AT_FDCWD, a + 2, &dir, &last, &ll);
    if (e)
      return e;
    struct inode *i = lookup_one(&dir, last, ll, &ex);
    if (!IS_ERR(i)) {
      path_put(&ex);
      path_put(&dir);
      return -EADDRINUSE;
    }
    if ((e = permission(dir.ino, MAY_WRITE | MAY_EXEC))) {
      path_put(&dir);
      return e;
    }
    i = dir.ino->iops->create(dir.ino, last, ll, S_IFSOCK | (0777 & ~cur_proc()->umask), 0);
    path_put(&dir);
    if (IS_ERR(i))
      return PTR_ERR(i) == -EEXIST ? -EADDRINUSE : PTR_ERR(i);
    i->sock = s;
    s->bound = i; /* keeps the inode (and its sock pointer) in memory */
  }
  memcpy(s->addr, a, (size_t)alen);
  s->addrlen = alen;
  return 0;
}

long sys_listen(long fd, long backlog) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  if (s->type == SOCK_DGRAM)
    return -EOPNOTSUPP;
  if (is_inet(s)) {
    if (s->state == SS_CONNECTED || s->state == SS_DISCONNECTED)
      return -EINVAL;
    if (s->state != SS_LISTENING) {
      if ((e = in_autobind(s)))
        return e;
      /* another listener on the address (both SO_REUSEADDR)? */
      s->state = SS_LISTENING;
      bool r = s->reuse;
      s->reuse = false;
      bool c = in_conflict(s, &s->la);
      s->reuse = r;
      if (c) {
        s->state = SS_UNCONNECTED;
        return -EADDRINUSE;
      }
    }
  } else if (s->state == SS_CONNECTED || !s->addrlen)
    return -EINVAL;
  s->state = SS_LISTENING;
  s->maxbacklog = backlog < 1 ? 1 : backlog > 4096 ? 4096 : (int)backlog;
  return 0;
}

static uint64_t to_deadline(uint64_t timeo) { return timeo ? sat_add(monotonic_ns(), timeo) : 0; }

long sys_connect(long fd, long uaddr, long len) {
  int e = -EBADF;
  struct file *f = fd_get((int)fd);
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  char a[SUN_PATH + 2];
  int alen;
  struct inaddr dst;
  struct sock *t;
  if (is_inet(s)) {
    uint16_t fam = 0;
    if (len >= 2 && copy_from_user(&fam, (void *)uaddr, 2) == 0 && fam == 0 && s->type == SOCK_DGRAM) {
      s->has_ra = false; /* AF_UNSPEC: disconnect */
      s->state = SS_UNCONNECTED;
      return 0;
    }
    if ((e = in_get(s, uaddr, len, &dst)))
      return e;
    if (s->type == SOCK_STREAM && s->tcb)
      return s->state == SS_CONNECTED ? -EISCONN : -EALREADY;
    if (s->type == SOCK_STREAM && s->state == SS_DISCONNECTED && !s->peer && !s->tcb && s->conn_failed) {
      if (s->error) { /* an earlier connect that failed: its error, once */
        e = s->error;
        s->error = 0;
        return -e;
      }
      /* then it may try again, as on Linux */
      s->state = SS_UNCONNECTED;
      s->shut_rd = s->shut_wr = false;
      s->conn_failed = false;
      s->has_ra = false;
    }
    int wire = in_dest(s, &dst);
    if (wire < 0)
      return wire;
    if (s->type == SOCK_DGRAM) {
      if (!s->raw && (e = in_autobind(s)))
        return e;
      if (!s->raw) { /* bound to the address it sends from, as Linux */
        struct inaddr src;
        in_src(s, &dst, &src);
        s->la = src;
      }
      s->ra = dst;
      s->has_ra = true;
      s->state = SS_CONNECTED;
      return 0;
    }
    if (s->state == SS_CONNECTED)
      return -EISCONN;
    if (s->state == SS_LISTENING || s->state == SS_DISCONNECTED)
      return -EINVAL;
    if (!dst.port)
      return -ECONNREFUSED;
    struct inaddr src;
    if (wire) { /* TCP through the network (net/tcp.c) */
      bool was_bound = s->in_bound;
      if ((e = in_autobind(s)))
        return e;
      s->autobound = !was_bound;
      in_src(s, &dst, &src);
      s->la = src;
      s->ra = dst;
      s->has_ra = true;
      e = tcp_connect(s, &src, &dst, (f->flags & O_NONBLOCK) != 0);
      if (e && e != -EINPROGRESS && e != -EINTR)
        connect_failed(s);
      return e;
    }
    in_src(s, &dst, &src);
    t = in_find(IPPROTO_TCP, &dst, &src);
    if (!t)
      return -ECONNREFUSED;
    if ((e = in_autobind(s))) {
      sock_put(t);
      return e;
    }
    goto stream;
  }
  if ((e = get_addr(uaddr, len, a, &alen))) {
    /* AF_UNSPEC on a datagram socket: disconnect */
    uint16_t fam = 0;
    if (s->type == SOCK_DGRAM && len >= 2 && copy_from_user(&fam, (void *)uaddr, 2) == 0 && fam == 0) {
      if (s->peer) {
        sock_put(s->peer);
        s->peer = NULL;
      }
      s->state = SS_UNCONNECTED;
      return 0;
    }
    return e;
  }
  t = lookup_addr(a, alen, &e);
  if (!t)
    return e;
  if (t->type != s->type) {
    sock_put(t);
    return -EPROTOTYPE;
  }
  if (s->type == SOCK_DGRAM) {
    if (s->peer)
      sock_put(s->peer);
    s->peer = t;
    s->state = SS_CONNECTED;
    return 0;
  }
  if (s->state == SS_CONNECTED) {
    sock_put(t);
    return -EISCONN;
  }
  if (s->state == SS_LISTENING) {
    sock_put(t);
    return -EINVAL;
  }
stream:;
  uint64_t fl = irq_save();
  while (t->state == SS_LISTENING && t->nbacklog >= t->maxbacklog) {
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      sock_put(t);
      return -EAGAIN;
    }
    if (sleep_on(&t->wq, 0, true) == -EINTR) {
      irq_restore(fl);
      sock_put(t);
      return -ERESTARTSYS;
    }
  }
  irq_restore(fl);
  if (t->state != SS_LISTENING) {
    sock_put(t);
    return -ECONNREFUSED;
  }
  /* a new socket for the server side, waiting in the backlog */
  struct sock *srv = sock_new(s->type);
  if (!srv) {
    sock_put(t);
    return -ENOMEM;
  }
  srv->peer = s;
  s->ref++;
  s->peer = srv;
  srv->ref++;
  s->state = srv->state = SS_CONNECTED;
  srv->peercred = my_cred();
  s->peercred.pid = 0;
  /* the server's credentials are those of whoever called listen: approximated
     by the bound socket's file owner */
  if (t->file && t->file->ino) {
    s->peercred.uid = t->file->ino->uid;
    s->peercred.gid = t->file->ino->gid;
  }
  memcpy(srv->addr, t->addr, (size_t)t->addrlen);
  srv->addrlen = t->addrlen;
  if (is_inet(s)) {
    /* the client from its port on the address it reaches the server from;
       the server side has the address it was reached at */
    srv->family = t->family;
    srv->proto = t->proto;
    srv->nodelay = t->nodelay;
    struct inaddr src;
    in_src(s, &dst, &src);
    s->la = src; /* connected: bound to the address it uses */
    srv->ra = src;
    srv->la = dst;
    s->ra = dst;
    s->has_ra = srv->has_ra = true;
    srv->peercred = my_cred();
  }
  srv->listener = t;
  list_add_tail(&t->backlog, &srv->blink);
  t->nbacklog++;
  sock_wake(t);
  sock_put(t);
  return 0;
}

static long do_accept(long fd, long uaddr, long ulen, long flags) {
  int e = -EBADF;
  struct file *f = fd_get((int)fd);
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  if (flags & ~(long)(SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_CLOFORK))
    return -EINVAL;
  if (s->type == SOCK_DGRAM)
    return -EOPNOTSUPP;
  if (s->state != SS_LISTENING)
    return -EINVAL;
  uint64_t fl = irq_save();
  uint64_t dl = to_deadline(s->rcvtimeo);
  while (list_empty(&s->backlog)) {
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      return -EAGAIN;
    }
    int r = sleep_on(&s->wq, dl, true);
    if (r == -EINTR) {
      irq_restore(fl);
      return -ERESTARTSYS;
    }
    if (r == -ETIMEDOUT) {
      irq_restore(fl);
      return -EAGAIN;
    }
  }
  struct sock *c = list_entry(s->backlog.next, struct sock, blink);
  list_del(&c->blink);
  s->nbacklog--;
  c->listener = NULL;
  irq_restore(fl);
  sock_wake(s);
  if (uaddr) {
    int32_t len;
    if (copy_from_user(&len, (void *)ulen, 4))
      return -EFAULT;
    char a[SUN_PATH + 2] = {AF_UNIX, 0};
    int al = 2;
    if (is_inet(c))
      al = in_put(c->family, &c->ra, a);
    else if (c->peer && c->peer->addrlen) {
      memcpy(a, c->peer->addr, (size_t)c->peer->addrlen);
      al = c->peer->addrlen;
    }
    if (len < 0)
      return -EINVAL;
    if (copy_to_user((void *)uaddr, a, (size_t)MIN(len, al)) || copy_to_user((void *)ulen, &al, 4))
      return -EFAULT;
  }
  return install(c, (int)flags);
}

long sys_accept(long fd, long uaddr, long ulen) { return do_accept(fd, uaddr, ulen, 0); }
long sys_accept4(long fd, long uaddr, long ulen, long flags) { return do_accept(fd, uaddr, ulen, flags); }

static long name_out(struct sock *s, bool peer, long uaddr, long ulen) {
  int32_t len;
  if (copy_from_user(&len, (void *)ulen, 4))
    return -EFAULT;
  if (len < 0)
    return -EINVAL;
  struct sock *x = peer ? s->peer : s;
  char a[SUN_PATH + 2] = {AF_UNIX, 0};
  int al = 2;
  if (is_inet(s)) {
    struct inaddr z;
    memset(&z, 0, sizeof(z));
    if (s->family == AF_INET)
      memcpy(z.a, v4pre, 12);
    if (peer && (!s->has_ra || (s->type == SOCK_STREAM && s->state != SS_CONNECTED)))
      return -ENOTCONN;
    al = in_put(s->family, peer ? &s->ra : (s->in_bound || s->has_ra) ? &s->la : &z, a);
  } else if (peer && (!x || s->state != SS_CONNECTED))
    return -ENOTCONN;
  else if (x->addrlen) {
    memcpy(a, x->addr, (size_t)x->addrlen);
    al = x->addrlen;
  }
  if (copy_to_user((void *)uaddr, a, (size_t)MIN(len, al)) || copy_to_user((void *)ulen, &al, 4))
    return -EFAULT;
  return 0;
}

long sys_getsockname(long fd, long uaddr, long ulen) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  return s ? name_out(s, false, uaddr, ulen) : e;
}

long sys_getpeername(long fd, long uaddr, long ulen) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  return s ? name_out(s, true, uaddr, ulen) : e;
}

long sys_shutdown(long fd, long how) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  if (how < 0 || how > 2)
    return -EINVAL;
  if (s->state != SS_CONNECTED && s->type != SOCK_DGRAM)
    return -ENOTCONN;
  if (how != SHUT_WR)
    s->shut_rd = true;
  if (how != SHUT_RD && s->tcb)
    tcp_shutdown_wr(s);
  if (how != SHUT_RD)
    s->shut_wr = true;
  if (s->peer && s->type != SOCK_DGRAM) {
    if (how != SHUT_RD)
      s->peer->shut_rd = true; /* end of file for the peer */
    sock_wake(s->peer);
  }
  sock_wake(s);
  return 0;
}

/* ---- sending and receiving ---- */

struct kmsghdr {
  uint64_t name;
  uint32_t namelen, pad0;
  uint64_t iov, iovlen, control, controllen;
  int32_t flags, pad1;
};

struct kiovec {
  uint64_t base, len;
};

static size_t queued(struct sock *s) { return s->rbytes; }

/* gather the user's buffers */
static long iov_total(struct kiovec *v, int n) {
  long t = 0;
  for (int i = 0; i < n; i++) {
    if ((int64_t)v[i].len < 0 || !user_range_ok(v[i].base, v[i].len))
      return -EFAULT;
    t += (long)v[i].len;
  }
  return t;
}

static int iov_copy_in(char *dst, struct kiovec *v, int n, size_t skip, size_t len) {
  for (int i = 0; i < n && len; i++) {
    if (skip >= v[i].len) {
      skip -= v[i].len;
      continue;
    }
    size_t c = MIN(v[i].len - skip, len);
    if (copy_from_user(dst, (char *)v[i].base + skip, c))
      return -EFAULT;
    dst += c;
    len -= c;
    skip = 0;
  }
  return 0;
}

static int iov_copy_out(struct kiovec *v, int n, size_t skip, const char *src, size_t len) {
  for (int i = 0; i < n && len; i++) {
    if (skip >= v[i].len) {
      skip -= v[i].len;
      continue;
    }
    size_t c = MIN(v[i].len - skip, len);
    if (copy_to_user((char *)v[i].base + skip, src, c))
      return -EFAULT;
    src += c;
    len -= c;
    skip = 0;
  }
  return 0;
}

/* the control messages of a send: files and credentials */
static int parse_control(uint64_t uctl, uint64_t len, struct skb *b) {
  uint64_t off = 0;
  while (off + 16 <= len) {
    uint64_t clen;
    int32_t lvl, type;
    if (copy_from_user(&clen, (char *)uctl + off, 8) || copy_from_user(&lvl, (char *)uctl + off + 8, 4) ||
        copy_from_user(&type, (char *)uctl + off + 12, 4))
      return -EFAULT;
    if (clen < 16 || off + clen > len)
      return -EINVAL;
    if (lvl == SOL_SOCKET && type == SCM_RIGHTS) {
      int n = (int)((clen - 16) / 4);
      if (n > 253)
        return -EINVAL;
      struct file **fds = krealloc(b->fds, (size_t)b->nfds * sizeof(*fds), (size_t)(b->nfds + n) * sizeof(*fds) + 8);
      if (!fds)
        return -ENOMEM;
      b->fds = fds;
      for (int i = 0; i < n; i++) {
        int32_t fd;
        if (copy_from_user(&fd, (char *)uctl + off + 16 + 4 * i, 4))
          return -EFAULT;
        struct file *f = fd_get(fd);
        if (!f)
          return -EBADF;
        file_get(f);
        b->fds[b->nfds++] = f;
      }
    } else if (lvl == SOL_SOCKET && type == SCM_CREDENTIALS) {
      struct ucred c;
      if (clen < 16 + sizeof(c) || copy_from_user(&c, (char *)uctl + off + 16, sizeof(c)))
        return -EINVAL;
      struct proc *p = cur_proc();
      if (!capable() && (c.pid != p->pid || (c.uid != p->cred.uid && c.uid != p->cred.euid) ||
                         (c.gid != p->cred.gid && c.gid != p->cred.egid)))
        return -EPERM;
      b->cred = c;
    } else
      return -EINVAL;
    off += (clen + 7) & ~7UL;
  }
  return 0;
}

/* ---- to and from the network (net/ip.c, net/tcp.c) ---- */

/* a TCP connection did not come about: the port connect() chose is free
   again and the socket may connect again (Linux does the same) */
void connect_failed(struct sock *s) {
  if (s->autobound && s->in_bound) {
    list_del(&s->ilink);
    list_init(&s->ilink);
    s->in_bound = false;
    memset(&s->la, 0, sizeof(s->la));
    if (s->family == AF_INET)
      s->la.a[10] = s->la.a[11] = 0xff;
  }
  s->autobound = false;
  s->conn_failed = true;
  s->state = SS_DISCONNECTED;
}

/* a UDP datagram from src (on this system) to dst, through the network */
static long udp_output(const struct inaddr *src, const struct inaddr *dst, struct kiovec *v, int nv, size_t len) {
  uint8_t *u = kmalloc(len + 8);
  if (!u)
    return -ENOBUFS;
  if (iov_copy_in((char *)u + 8, v, nv, 0, len)) {
    kfree(u);
    return -EFAULT;
  }
  uint32_t s = in_ip4(src), d = in_ip4(dst);
  u[0] = (uint8_t)(src->port >> 8), u[1] = (uint8_t)src->port;
  u[2] = (uint8_t)(dst->port >> 8), u[3] = (uint8_t)dst->port;
  u[4] = (uint8_t)((len + 8) >> 8), u[5] = (uint8_t)(len + 8);
  u[6] = u[7] = 0;
  uint16_t c = ip_csum(u, len + 8, ip_pseudo(s, d, IPPROTO_UDP, len + 8));
  c = c ? c : 0xffff;
  u[6] = (uint8_t)(c >> 8), u[7] = (uint8_t)c;
  int r = ip_output(IPPROTO_UDP, s, d, u, len + 8);
  kfree(u);
  return r ? r : (long)len;
}

void udp_deliver(struct sock *t, const struct inaddr *src, const uint8_t *data, size_t n) {
  if (t->rbytes + n > SOCK_BUF || t->rcount >= MAX_DGRAMS)
    return; /* full: dropped */
  struct skb *b = kzalloc(sizeof(*b) + n + 1);
  if (!b)
    return;
  b->len = n;
  b->eor = true;
  memcpy(b->data, data, n);
  b->fromlen = in_put(t->family, src, b->from);
  uint64_t fl = irq_save();
  list_add_tail(&t->rq, &b->link);
  t->rbytes += n;
  t->rcount++;
  irq_restore(fl);
  sock_wake(t);
}

/* an ICMP error about a datagram from local to remote: a connected socket
   hears of it (POSIX leaves the rest alone) */
void udp_icmp_error(const struct inaddr *local, const struct inaddr *remote, int err) {
  list_for_each(p, &inet_bound) {
    struct sock *x = list_entry(p, struct sock, ilink);
    if (x->proto == IPPROTO_UDP && x->la.port == local->port && x->has_ra && in_same(&x->ra, remote) &&
        x->ra.port == remote->port) {
      x->error = err;
      sock_wake(x);
    }
  }
}

/* stream data from the network for s */
bool sock_queue(struct sock *s, const void *data, size_t n) {
  if (s->rbytes + n > SOCK_BUF)
    return false;
  struct skb *b = kzalloc(sizeof(*b) + n + 1);
  if (!b)
    return false;
  b->len = n;
  memcpy(b->data, data, n);
  uint64_t fl = irq_save();
  list_add_tail(&s->rq, &b->link);
  s->rbytes += n;
  s->rcount++;
  irq_restore(fl);
  sock_wake(s);
  return true;
}

bool sock_backlog_full(struct sock *l) { return l->nbacklog >= l->maxbacklog; }

void sock_add_backlog(struct sock *l, struct sock *c) {
  c->listener = l;
  list_add_tail(&l->backlog, &c->blink);
  l->nbacklog++;
  sock_wake(l);
}

/* ---- raw sockets ---- */

static uint16_t csum(const uint8_t *p, size_t n, uint32_t sum) {
  for (; n > 1; p += 2, n -= 2)
    sum += (uint32_t)(p[0] << 8 | p[1]);
  if (n)
    sum += (uint32_t)(p[0] << 8);
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);
  return (uint16_t)~sum;
}

/* the ICMPv6 checksum of msg (n bytes) from src to dst, put in place */
static void icmp6_sum(const struct inaddr *src, const struct inaddr *dst, uint8_t *msg, size_t n) {
  uint8_t ph[40];
  memcpy(ph, src->a, 16);
  memcpy(ph + 16, dst->a, 16);
  ph[32] = (uint8_t)(n >> 24), ph[33] = (uint8_t)(n >> 16), ph[34] = (uint8_t)(n >> 8), ph[35] = (uint8_t)n;
  ph[36] = ph[37] = ph[38] = 0, ph[39] = IPPROTO_ICMPV6;
  msg[2] = msg[3] = 0;
  uint32_t s = (uint32_t)(uint16_t)~csum(ph, 40, 0);
  uint16_t c = csum(msg, n, s);
  msg[2] = (uint8_t)(c >> 8), msg[3] = (uint8_t)c;
}

/* a packet of protocol proto from src to dst (IPv4: pkt is the whole
   packet, header included; IPv6: the payload) to the raw sockets */
void raw_deliver(int proto, const struct inaddr *src, const struct inaddr *dst, const uint8_t *pkt, size_t n) {
  bool v4 = in_v4(dst);
  list_for_each(p, &raw_socks) {
    struct sock *x = list_entry(p, struct sock, ilink);
    if (x->proto != proto || (x->family == AF_INET) != v4)
      continue;
    if (x->in_bound && !in_covers(&x->la, x->v6only, dst))
      continue;
    if (x->has_ra && !in_same(&x->ra, src))
      continue;
    if (x->rbytes + n > SOCK_BUF || x->rcount >= MAX_DGRAMS)
      continue; /* full: dropped */
    struct skb *b = kzalloc(sizeof(*b) + n + 1);
    if (!b)
      continue;
    b->len = n;
    b->eor = true;
    memcpy(b->data, pkt, n);
    struct inaddr from = *src;
    from.port = 0;
    b->fromlen = in_put(x->family, &from, b->from);
    uint64_t fl = irq_save();
    list_add_tail(&x->rq, &b->link);
    x->rbytes += n;
    x->rcount++;
    irq_restore(fl);
    sock_wake(x);
  }
}

static void ip4_header(uint8_t *h, size_t total, int proto, const struct inaddr *src, const struct inaddr *dst) {
  static uint16_t id;
  memset(h, 0, 20);
  h[0] = 0x45;
  h[2] = (uint8_t)(total >> 8), h[3] = (uint8_t)total;
  id++;
  h[4] = (uint8_t)(id >> 8), h[5] = (uint8_t)id;
  h[6] = 0x40; /* don't fragment */
  h[8] = 64;
  h[9] = (uint8_t)proto;
  memcpy(h + 12, src->a + 12, 4);
  memcpy(h + 16, dst->a + 12, 4);
  uint16_t c = csum(h, 20, 0);
  h[10] = (uint8_t)(c >> 8), h[11] = (uint8_t)c;
}

/* a packet, then what the kernel answers (echo requests) */
static void raw_packet(int proto, const struct inaddr *src, const struct inaddr *dst, uint8_t *pkt, size_t n) {
  raw_deliver(proto, src, dst, pkt, n);
  bool v4 = in_v4(dst);
  size_t hl = v4 ? (size_t)(pkt[0] & 15) * 4 : 0;
  if (n < hl + 8)
    return;
  uint8_t *icmp = pkt + hl;
  if (v4 && proto == IPPROTO_ICMP && icmp[0] == 8 && icmp[1] == 0) {
    uint8_t *r = kmalloc(n - hl + 20);
    if (!r)
      return;
    memcpy(r + 20, icmp, n - hl);
    r[20] = 0; /* echo reply */
    r[22] = r[23] = 0;
    uint16_t c = csum(r + 20, n - hl, 0);
    r[22] = (uint8_t)(c >> 8), r[23] = (uint8_t)c;
    ip4_header(r, n - hl + 20, IPPROTO_ICMP, dst, src);
    raw_deliver(IPPROTO_ICMP, dst, src, r, n - hl + 20);
    kfree(r);
  } else if (!v4 && proto == IPPROTO_ICMPV6 && icmp[0] == 128 && icmp[1] == 0) {
    uint8_t *r = kmalloc(n);
    if (!r)
      return;
    memcpy(r, icmp, n);
    r[0] = 129;
    icmp6_sum(dst, src, r, n);
    raw_deliver(IPPROTO_ICMPV6, dst, src, r, n);
    kfree(r);
  }
}

static long raw_send(struct sock *s, struct kmsghdr *m, struct kiovec *vp, long total) {
  struct inaddr dst, src;
  int e;
  if (m->name && m->namelen) {
    if ((e = in_get(s, (long)m->name, (long)m->namelen, &dst)))
      return e;
  } else if (s->has_ra)
    dst = s->ra;
  else
    return -EDESTADDRREQ;
  int wire = in_dest(s, &dst);
  if (wire < 0)
    return wire;
  dst.port = 0;
  bool v4 = in_v4(&dst);
  if (total > 65535 - (v4 && !s->hdrincl ? 20 : 0))
    return -EMSGSIZE;
  if (s->shut_wr)
    return -EPIPE;
  in_src(s, &dst, &src);
  src.port = 0;
  size_t hl = v4 && !s->hdrincl ? 20 : 0, n = hl + (size_t)total;
  uint8_t *pkt = kmalloc(n + 1);
  if (!pkt)
    return -ENOMEM;
  if (iov_copy_in((char *)pkt + hl, vp, (int)m->iovlen, 0, (size_t)total)) {
    kfree(pkt);
    return -EFAULT;
  }
  int proto = s->proto;
  if (!v4 && proto == IPPROTO_ICMPV6 && total >= 4)
    icmp6_sum(&src, &dst, pkt, n);
  if (hl)
    ip4_header(pkt, n, proto, &src, &dst);
  else if (v4) { /* the sender's header: its protocol, length and checksum */
    if (n < 20 || (pkt[0] >> 4) != 4) {
      kfree(pkt);
      return -EINVAL;
    }
    proto = pkt[9];
    pkt[2] = (uint8_t)(n >> 8), pkt[3] = (uint8_t)n;
    pkt[10] = pkt[11] = 0;
    uint16_t c = csum(pkt, (size_t)(pkt[0] & 15) * 4, 0);
    pkt[10] = (uint8_t)(c >> 8), pkt[11] = (uint8_t)c;
  }
  if (wire) {
    e = hl ? ip_output(proto, in_ip4(&src), in_ip4(&dst), pkt + hl, (size_t)total) : ip_output_raw(pkt, n);
    kfree(pkt);
    return e ? e : total;
  }
  raw_packet(proto, &src, &dst, pkt, n);
  kfree(pkt);
  return total;
}

static long do_send(struct file *f, struct kmsghdr *m, int flags) {
  int e = -EBADF;
  struct sock *s = SK(f);
  if (m->iovlen > 1024)
    return -EMSGSIZE;
  struct kiovec v[64];
  struct kiovec *vp = m->iovlen > 64 ? kmalloc(m->iovlen * sizeof(*vp)) : v;
  if (!vp)
    return -ENOMEM;
  long r;
  if (copy_from_user(vp, (void *)m->iov, m->iovlen * sizeof(*vp))) {
    r = -EFAULT;
    goto out;
  }
  long total = iov_total(vp, (int)m->iovlen);
  if (total < 0) {
    r = total;
    goto out;
  }
  bool nonblock = (f->flags & O_NONBLOCK) || (flags & MSG_DONTWAIT);
  struct sock *t;
  if (flags & MSG_OOB) {
    r = -EOPNOTSUPP;
    goto out;
  }
  if (s->raw) {
    r = raw_send(s, m, vp, total);
    goto out;
  }
  bool inet_dg = s->type == SOCK_DGRAM && is_inet(s);
  struct inaddr isrc;
  if (inet_dg) {
    struct inaddr dst;
    if (s->error) {
      r = -s->error;
      s->error = 0;
      goto out;
    }
    if (m->name && m->namelen) {
      if ((e = in_get(s, (long)m->name, (long)m->namelen, &dst))) {
        r = e;
        goto out;
      }
    } else if (s->has_ra)
      dst = s->ra;
    else {
      r = -EDESTADDRREQ;
      goto out;
    }
    int wire = in_dest(s, &dst);
    if (wire < 0) {
      r = wire;
      goto out;
    }
    if (!dst.port) {
      r = -EINVAL;
      goto out;
    }
    if (total > 65507) {
      r = -EMSGSIZE;
      goto out;
    }
    if ((e = in_autobind(s))) {
      r = e;
      goto out;
    }
    if (s->shut_wr) {
      r = -EPIPE;
      goto out;
    }
    in_src(s, &dst, &isrc);
    if (wire) {
      r = udp_output(&isrc, &dst, vp, (int)m->iovlen, (size_t)total);
      goto out;
    }
    t = in_find(IPPROTO_UDP, &dst, &isrc);
    if (!t) { /* nobody there: lost (a connected socket hears of it) */
      if (s->has_ra)
        s->error = ECONNREFUSED;
      r = total;
      goto out;
    }
  } else if (s->type == SOCK_DGRAM) {
    if (m->name) {
      char a[SUN_PATH + 2];
      int alen;
      if ((e = get_addr((long)m->name, (long)m->namelen, a, &alen))) {
        r = e;
        goto out;
      }
      t = lookup_addr(a, alen, &e);
      if (!t) {
        r = e;
        goto out;
      }
      if (t->type != SOCK_DGRAM) {
        sock_put(t);
        r = -EPROTOTYPE;
        goto out;
      }
    } else {
      t = s->peer;
      if (!t) {
        r = -ENOTCONN;
        goto out;
      }
      t->ref++;
    }
    if (s->shut_wr) {
      sock_put(t);
      r = -EPIPE;
      goto out;
    }
    if (total > SOCK_BUF / 2) {
      sock_put(t);
      r = -EMSGSIZE;
      goto out;
    }
  } else if (s->type == SOCK_STREAM && is_inet(s) && (s->tcb || (s->state == SS_DISCONNECTED && !s->peer))) {
    /* TCP through the network */
    long sent = 0;
    for (size_t i = 0; i < m->iovlen; i++) {
      long n = tcp_send(s, (const char *)vp[i].base, vp[i].len, nonblock, flags);
      if (n < 0) {
        r = sent ? sent : n;
        goto out;
      }
      sent += n;
      if ((uint64_t)n < vp[i].len)
        break;
    }
    r = sent;
    goto out;
  } else {
    if (s->state != SS_CONNECTED || !s->peer) {
      r = s->state == SS_DISCONNECTED ? -EPIPE : -ENOTCONN;
      if (r == -EPIPE && !(flags & MSG_NOSIGNAL))
        signal_simple(cur_proc(), SIGPIPE);
      goto out;
    }
    if (m->name && s->type == SOCK_STREAM && !is_inet(s)) {
      r = -EISCONN; /* (TCP ignores the address, as POSIX says) */
      goto out;
    }
    t = s->peer;
    t->ref++;
    if (s->shut_wr) {
      sock_put(t);
      if (!(flags & MSG_NOSIGNAL))
        signal_simple(cur_proc(), SIGPIPE);
      r = -EPIPE;
      goto out;
    }
    if (s->type == SOCK_SEQPACKET && total > SOCK_BUF / 2) {
      sock_put(t);
      r = -EMSGSIZE;
      goto out;
    }
  }
  /* stream data is sent in pieces that fit; records whole */
  size_t sent = 0;
  bool first = true;
  uint64_t dl = to_deadline(s->sndtimeo);
  do {
    uint64_t fl = irq_save();
    size_t want = (size_t)total - sent;
    for (;;) {
      if (s->type == SOCK_STREAM && (t->state == SS_DISCONNECTED || t->shut_rd || s->peer != t)) {
        irq_restore(fl);
        sock_put(t);
        if (sent) {
          r = (long)sent;
          goto out;
        }
        if (!(flags & MSG_NOSIGNAL))
          signal_simple(cur_proc(), SIGPIPE);
        r = -EPIPE;
        goto out;
      }
      size_t space = SOCK_BUF > queued(t) ? SOCK_BUF - queued(t) : 0;
      bool fits = s->type == SOCK_STREAM ? space > 0 && (space >= MIN(want, 4096UL)) : space >= want && t->rcount < MAX_DGRAMS;
      if (fits)
        break;
      if (inet_dg) { /* a full receiver drops datagrams */
        irq_restore(fl);
        sock_put(t);
        r = total;
        goto out;
      }
      if (nonblock) {
        irq_restore(fl);
        sock_put(t);
        r = sent ? (long)sent : -EAGAIN;
        goto out;
      }
      int sr = sleep_on(&t->wq, dl, true);
      if (sr) {
        irq_restore(fl);
        sock_put(t);
        r = sent ? (long)sent : sr == -ETIMEDOUT ? -EAGAIN : -ERESTARTSYS;
        goto out;
      }
    }
    irq_restore(fl);
    size_t chunk = want;
    if (s->type == SOCK_STREAM)
      chunk = MIN(want, SOCK_BUF - queued(t));
    struct skb *b = kzalloc(sizeof(*b) + chunk + 1);
    if (!b) {
      sock_put(t);
      r = sent ? (long)sent : -ENOMEM;
      goto out;
    }
    b->len = chunk;
    b->cred = my_cred();
    b->eor = true;
    if (inet_dg)
      b->fromlen = in_put(t->family, &isrc, b->from);
    else if (s->addrlen) {
      memcpy(b->from, s->addr, (size_t)s->addrlen);
      b->fromlen = s->addrlen;
    }
    if (first && m->control && m->controllen) {
      if ((e = parse_control(m->control, m->controllen, b))) {
        skb_free(b);
        sock_put(t);
        r = e;
        goto out;
      }
    }
    if (iov_copy_in(b->data, vp, (int)m->iovlen, sent, chunk)) {
      skb_free(b);
      sock_put(t);
      r = sent ? (long)sent : -EFAULT;
      goto out;
    }
    fl = irq_save();
    list_add_tail(&t->rq, &b->link);
    t->rbytes += chunk;
    t->rcount++;
    irq_restore(fl);
    sock_wake(t);
    sent += chunk;
    first = false;
  } while (sent < (size_t)total);
  sock_put(t);
  r = (long)sent;
out:
  if (vp != v)
    kfree(vp);
  return r;
}

static int put_control(struct kmsghdr *m, struct skb *b, unsigned fdfl, int *mflags, uint64_t *used, bool cred) {
  uint64_t off = *used;
  if (cred) {
    uint64_t clen = 16 + sizeof(struct ucred);
    if (off + clen > m->controllen)
      *mflags |= MSG_CTRUNC;
    else {
      int32_t lt[2] = {SOL_SOCKET, SCM_CREDENTIALS};
      if (copy_to_user((char *)m->control + off, &clen, 8) || copy_to_user((char *)m->control + off + 8, lt, 8) ||
          copy_to_user((char *)m->control + off + 16, &b->cred, sizeof(b->cred)))
        return -EFAULT;
      off += (clen + 7) & ~7UL;
    }
  }
  if (b->nfds) {
    int room = m->controllen > off + 16 ? (int)((m->controllen - off - 16) / 4) : 0;
    int n = MIN(room, b->nfds);
    if (n < b->nfds)
      *mflags |= MSG_CTRUNC;
    if (n > 0) {
      uint64_t clen = 16 + 4 * (uint64_t)n;
      int32_t lt[2] = {SOL_SOCKET, SCM_RIGHTS};
      if (copy_to_user((char *)m->control + off, &clen, 8) || copy_to_user((char *)m->control + off + 8, lt, 8))
        return -EFAULT;
      for (int i = 0; i < n; i++) {
        file_get(b->fds[i]);
        int fd = fd_install(b->fds[i], 0, fdfl);
        if (fd < 0) {
          file_put(b->fds[i]);
          *mflags |= MSG_CTRUNC;
          fd = -1;
        }
        int32_t v = fd;
        copy_to_user((char *)m->control + off + 16 + 4 * i, &v, 4);
      }
      off += (clen + 7) & ~7UL;
    }
    /* the files went to the receiver (or are dropped) */
    for (int i = 0; i < b->nfds; i++)
      file_put(b->fds[i]);
    kfree(b->fds);
    b->fds = NULL;
    b->nfds = 0;
  }
  *used = off;
  return 0;
}

static long do_recv(struct file *f, struct kmsghdr *m, int flags) {
  struct sock *s = SK(f);
  if (flags & MSG_OOB)
    return -EOPNOTSUPP;
  if (m->iovlen > 1024)
    return -EMSGSIZE;
  struct kiovec v[64];
  struct kiovec *vp = m->iovlen > 64 ? kmalloc(m->iovlen * sizeof(*vp)) : v;
  if (!vp)
    return -ENOMEM;
  long r;
  if (copy_from_user(vp, (void *)m->iov, m->iovlen * sizeof(*vp))) {
    r = -EFAULT;
    goto out;
  }
  long total = iov_total(vp, (int)m->iovlen);
  if (total < 0) {
    r = total;
    goto out;
  }
  if (s->type != SOCK_DGRAM && s->state != SS_CONNECTED && s->state != SS_DISCONNECTED) {
    r = -ENOTCONN;
    goto out;
  }
  bool nonblock = (f->flags & O_NONBLOCK) || (flags & MSG_DONTWAIT);
  uint64_t dl = to_deadline(s->rcvtimeo);
  uint64_t fl = irq_save();
  while (list_empty(&s->rq)) {
    if (s->error) {
      r = -s->error;
      s->error = 0;
      irq_restore(fl);
      goto out;
    }
    if (s->shut_rd || (s->type != SOCK_DGRAM && (s->state == SS_DISCONNECTED || (!s->peer && !s->tcb)))) {
      irq_restore(fl);
      r = 0;
      goto out;
    }
    if (nonblock) {
      irq_restore(fl);
      r = -EAGAIN;
      goto out;
    }
    int sr = sleep_on(&s->wq, dl, true);
    if (sr) {
      irq_restore(fl);
      r = sr == -ETIMEDOUT ? -EAGAIN : -ERESTARTSYS;
      goto out;
    }
  }
  irq_restore(fl);
  int mflags = 0;
  uint64_t cused = 0;
  size_t got = 0;
  bool peek = flags & MSG_PEEK;
  struct list *pos = s->rq.next;
  while (pos != &s->rq) {
    struct skb *b = list_entry(pos, struct skb, link);
    if (got && (b->nfds || s->type != SOCK_STREAM))
      break; /* records, and files, are not merged */
    if (got == 0 && m->name && m->namelen && !(is_inet(s) && s->type == SOCK_STREAM)) {
      char a[SUN_PATH + 2] = {AF_UNIX, 0};
      int al = 2;
      if (b->fromlen) {
        memcpy(a, b->from, (size_t)b->fromlen);
        al = b->fromlen;
      }
      if (copy_to_user((void *)m->name, a, MIN((size_t)m->namelen, (size_t)al)))
        r = -EFAULT;
      m->namelen = (uint32_t)al;
    } else if (got == 0)
      m->namelen = 0;
    if (got == 0 && m->control && (b->nfds || s->passcred)) {
      unsigned fdfl = ((flags & MSG_CMSG_CLOEXEC) ? FD_CLOEXEC : 0) | (((unsigned)flags & MSG_CMSG_CLOFORK) ? FD_CLOFORK : 0);
      if (!peek && put_control(m, b, fdfl, &mflags, &cused, s->passcred)) {
        r = -EFAULT;
        goto out;
      }
    } else if (got == 0 && b->nfds && !peek) {
      mflags |= MSG_CTRUNC;
      for (int i = 0; i < b->nfds; i++)
        file_put(b->fds[i]);
      kfree(b->fds);
      b->fds = NULL;
      b->nfds = 0;
    }
    size_t avail = b->len - b->off;
    size_t c = MIN(avail, (size_t)total - got);
    if (iov_copy_out(vp, (int)m->iovlen, got, b->data + b->off, c)) {
      r = got ? (long)got : -EFAULT;
      goto out;
    }
    got += c;
    struct list *next = pos->next;
    if (s->type != SOCK_STREAM) {
      if (c < avail) {
        mflags |= MSG_TRUNC;
        if (flags & MSG_TRUNC)
          got = avail;
      }
      if (!peek) {
        list_del(&b->link);
        s->rbytes -= b->len;
        s->rcount--;
        skb_free(b);
      }
      break;
    }
    if (!peek) {
      b->off += c;
      s->rbytes -= c;
      if (b->off == b->len) {
        list_del(&b->link);
        s->rcount--;
        skb_free(b);
      }
    }
    if (got == (size_t)total)
      break;
    pos = next;
    /* MSG_WAITALL: wait for more */
    if (pos == &s->rq && (flags & MSG_WAITALL) && !peek && !nonblock) {
      fl = irq_save();
      while (list_empty(&s->rq) && !s->shut_rd && (s->peer || s->tcb) && s->state == SS_CONNECTED) {
        if (sleep_on(&s->wq, dl, true)) {
          irq_restore(fl);
          r = (long)got;
          goto wake;
        }
      }
      irq_restore(fl);
      pos = s->rq.next;
    }
  }
  if (m->control)
    m->controllen = cused;
  m->flags = mflags;
  r = (long)got;
wake:
  if (s->peer)
    sock_wake(s->peer);
  if (s->tcb)
    tcp_read_done(s);
  sock_wake(s);
out:
  if (vp != v)
    kfree(vp);
  return r;
}

long sys_sendmsg(long fd, long umsg, long flags) {
  struct kmsghdr m;
  if (copy_from_user(&m, (void *)umsg, sizeof(m)))
    return -EFAULT;
  int e = -EBADF;
  struct file *f = sock_file_of((int)fd, &e);
  return f ? do_send(f, &m, (int)flags) : e;
}

long sys_recvmsg(long fd, long umsg, long flags) {
  struct kmsghdr m;
  if (copy_from_user(&m, (void *)umsg, sizeof(m)))
    return -EFAULT;
  int e = -EBADF;
  struct file *f = sock_file_of((int)fd, &e);
  if (!f)
    return e;
  long r = do_recv(f, &m, (int)flags);
  if (r >= 0) {
    struct kmsghdr *u = (struct kmsghdr *)umsg;
    if (copy_to_user(&u->namelen, &m.namelen, 4) || copy_to_user(&u->controllen, &m.controllen, 8) ||
        copy_to_user(&u->flags, &m.flags, 4))
      return -EFAULT;
  }
  return r;
}

long sys_sendto(long fd, long buf, long len, long flags, long uaddr, long alen) {
  struct kiovec v = {(uint64_t)buf, (uint64_t)len};
  struct kmsghdr m;
  memset(&m, 0, sizeof(m));
  m.iov = (uint64_t)&v;
  m.iovlen = 1;
  m.name = (uint64_t)uaddr;
  m.namelen = (uint32_t)alen;
  int e = -EBADF;
  struct file *f = sock_file_of((int)fd, &e);
  return f ? do_send(f, &m, (int)flags) : e;
}

long sys_recvfrom(long fd, long buf, long len, long flags, long uaddr, long ulen) {
  struct kiovec v = {(uint64_t)buf, (uint64_t)len};
  struct kmsghdr m;
  memset(&m, 0, sizeof(m));
  m.iov = (uint64_t)&v;
  m.iovlen = 1;
  int32_t al = 0;
  if (uaddr) {
    if (copy_from_user(&al, (void *)ulen, 4))
      return -EFAULT;
    m.name = (uint64_t)uaddr;
    m.namelen = (uint32_t)al;
  }
  int e = -EBADF;
  struct file *f = sock_file_of((int)fd, &e);
  if (!f)
    return e;
  long r = do_recv(f, &m, (int)flags);
  if (r >= 0 && uaddr) {
    int32_t n = (int32_t)m.namelen;
    if (copy_to_user((void *)ulen, &n, 4))
      return -EFAULT;
  }
  return r;
}

/* ---- options ---- */

long sys_setsockopt(long fd, long level, long opt, long uval, long len) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  int32_t v = 0;
  if (level != SOL_SOCKET) {
    /* internet levels: TCP_NODELAY and IPV6_V6ONLY mean something here;
       the rest (TOS, TTL, keepalive times, ...) is accepted */
    bool ok = (level == IPPROTO_TCP && s->proto == IPPROTO_TCP) ||
              (level == IPPROTO_IP && is_inet(s)) || (level == IPPROTO_IPV6 && s->family == AF_INET6);
    if (!ok)
      return -ENOPROTOOPT;
    if (len < 4 || copy_from_user(&v, (void *)uval, 4))
      return len < 4 ? -EINVAL : -EFAULT;
    if (level == IPPROTO_TCP && opt == TCP_NODELAY)
      s->nodelay = v != 0;
    else if (level == IPPROTO_IP && opt == IP_HDRINCL && s->raw)
      s->hdrincl = v != 0 || s->proto == IPPROTO_RAW;
    else if (level == IPPROTO_IPV6 && opt == IPV6_V6ONLY) {
      if (s->in_bound)
        return -EINVAL;
      s->v6only = v != 0;
    }
    return 0;
  }
  if (opt == SO_RCVTIMEO || opt == SO_SNDTIMEO) {
    int64_t tv[2];
    if (len < 16 || copy_from_user(tv, (void *)uval, 16))
      return -EINVAL;
    if (tv[1] < 0 || tv[1] >= 1000000)
      return -EDOM;
    uint64_t ns = (uint64_t)tv[0] * 1000000000UL + (uint64_t)tv[1] * 1000;
    if (opt == SO_RCVTIMEO)
      s->rcvtimeo = ns;
    else
      s->sndtimeo = ns;
    return 0;
  }
  if (len >= 4 && copy_from_user(&v, (void *)uval, 4))
    return -EFAULT;
  switch (opt) {
  case SO_PASSCRED:
    s->passcred = v != 0;
    return 0;
  case SO_REUSEADDR:
  case SO_REUSEPORT:
    s->reuse = v != 0;
    return 0;
  case SO_KEEPALIVE:
    s->keepalive = v != 0;
    return 0;
  case SO_BROADCAST:
    s->broadcast = v != 0;
    return 0;
  case SO_LINGER:
    if (len < 8 || copy_from_user(s->linger, (void *)uval, 8))
      return -EINVAL;
    return 0;
  case SO_SNDBUF:
  case SO_RCVBUF:
  case SO_DEBUG:
  case SO_DONTROUTE:
  case SO_OOBINLINE:
  case SO_RCVLOWAT:
  case SO_SNDLOWAT:
    return 0;
  }
  return -ENOPROTOOPT;
}

long sys_getsockopt(long fd, long level, long opt, long uval, long ulen) {
  int e = -EBADF;
  struct sock *s = sock_of((int)fd, &e);
  if (!s)
    return e;
  int32_t len;
  if (copy_from_user(&len, (void *)ulen, 4))
    return -EFAULT;
  if (len < 0)
    return -EINVAL;
  char buf[16];
  int n = 4;
  int32_t v = 0;
  if (level != SOL_SOCKET) {
    if (level == IPPROTO_TCP && s->proto == IPPROTO_TCP)
      v = opt == TCP_NODELAY ? s->nodelay : opt == 2 /* TCP_MAXSEG */ ? 65483 : 0;
    else if (level == IPPROTO_IPV6 && s->family == AF_INET6)
      v = opt == IPV6_V6ONLY ? s->v6only : opt == 16 /* IPV6_UNICAST_HOPS */ ? 64 : 0;
    else if (level == IPPROTO_IP && is_inet(s))
      v = opt == 2 /* IP_TTL */ ? 64 : opt == IP_HDRINCL ? s->hdrincl : 0;
    else
      return -ENOPROTOOPT;
    int out = MIN(len, 4);
    if (copy_to_user((void *)uval, &v, (size_t)out) || copy_to_user((void *)ulen, &out, 4))
      return -EFAULT;
    return 0;
  }
  switch (opt) {
  case SO_TYPE: v = s->raw ? SOCK_RAW : s->type; break;
  case SO_ERROR: v = s->error; s->error = 0; break;
  case SO_SNDBUF:
  case SO_RCVBUF: v = SOCK_BUF; break;
  case SO_ACCEPTCONN: v = s->state == SS_LISTENING; break;
  case SO_PASSCRED: v = s->passcred; break;
  case SO_DOMAIN: v = s->netlink ? AF_NETLINK : s->family; break;
  case SO_PROTOCOL: v = s->proto; break;
  case SO_REUSEADDR:
  case SO_REUSEPORT: v = s->reuse; break;
  case SO_KEEPALIVE: v = s->keepalive; break;
  case SO_BROADCAST: v = s->broadcast; break;
  case SO_DEBUG:
  case SO_DONTROUTE:
  case SO_OOBINLINE: v = 0; break;
  case SO_RCVLOWAT:
  case SO_SNDLOWAT: v = 1; break;
  case SO_PEERCRED:
    if (s->type == SOCK_DGRAM && !s->peer)
      return -ENOTCONN;
    memcpy(buf, &s->peercred, 12);
    n = 12;
    break;
  case SO_RCVTIMEO:
  case SO_SNDTIMEO: {
    uint64_t ns = opt == SO_RCVTIMEO ? s->rcvtimeo : s->sndtimeo;
    int64_t tv[2] = {(int64_t)(ns / 1000000000UL), (int64_t)(ns % 1000000000UL / 1000)};
    memcpy(buf, tv, 16);
    n = 16;
    break;
  }
  case SO_LINGER:
    memcpy(buf, s->linger, 8);
    n = 8;
    break;
  default:
    return -ENOPROTOOPT;
  }
  if (n == 4)
    memcpy(buf, &v, 4);
  int out = MIN(len, n);
  if (copy_to_user((void *)uval, buf, (size_t)out) || copy_to_user((void *)ulen, &out, 4))
    return -EFAULT;
  return 0;
}

/* ---- the file side ---- */

static ssize_t sock_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct kiovec v = {(uint64_t)buf, n};
  struct kmsghdr m;
  memset(&m, 0, sizeof(m));
  m.iov = (uint64_t)&v;
  m.iovlen = 1;
  return do_recv(f, &m, 0);
}

static ssize_t sock_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct kiovec v = {(uint64_t)buf, n};
  struct kmsghdr m;
  memset(&m, 0, sizeof(m));
  m.iov = (uint64_t)&v;
  m.iovlen = 1;
  return do_send(f, &m, 0);
}

static unsigned sock_poll(struct file *f) {
  struct sock *s = SK(f);
  unsigned m = 0;
  if (s->state == SS_LISTENING)
    return list_empty(&s->backlog) ? 0 : POLLIN | POLLRDNORM;
  if (!list_empty(&s->rq))
    m |= POLLIN | POLLRDNORM;
  if (s->shut_rd)
    m |= POLLIN | POLLRDNORM | 0x2000; /* POLLRDHUP */
  if (s->error)
    m |= POLLERR;
  if (s->type == SOCK_STREAM && is_inet(s) && (s->tcb || (s->state == SS_DISCONNECTED && !s->peer))) {
    if (!s->tcb) /* over (or failed to connect) */
      m |= POLLHUP | POLLOUT | POLLWRNORM;
    else if (tcp_writable(s))
      m |= POLLOUT | POLLWRNORM;
    if (s->shut_rd && s->shut_wr)
      m |= POLLHUP;
    return m;
  }
  if (s->type != SOCK_DGRAM) {
    if (s->state == SS_DISCONNECTED || (s->shut_rd && s->shut_wr))
      m |= POLLHUP;
    if (s->state == SS_CONNECTED && s->peer && queued(s->peer) < SOCK_BUF && !s->shut_wr)
      m |= POLLOUT | POLLWRNORM;
  } else {
    struct sock *t = s->peer;
    if (is_inet(s) || !t || queued(t) < SOCK_BUF)
      m |= POLLOUT | POLLWRNORM;
  }
  return m;
}

static void sock_release(struct file *f) {
  struct sock *s = SK(f);
  if (!s)
    return;
  sock_close(s);
  sock_put(s);
  f->priv = NULL;
}

#define FIONREAD 0x541b
#define SIOCOUTQ 0x5411
#define SIOCATMARK 0x8905
#define SIOCGIFNAME 0x8910
#define SIOCGIFCONF 0x8912
#define SIOCGIFFLAGS 0x8913
#define SIOCGIFADDR 0x8915
#define SIOCGIFNETMASK 0x891b
#define SIOCGIFMTU 0x8921
#define SIOCGIFINDEX 0x8933
#define SIOCGIFBRDADDR 0x8919
#define SIOCGIFHWADDR 0x8927

/* the interfaces: lo, index 1 */
/* struct ifreq for interface i (1 lo, 2 eth0): its name, and for cmd its value */
static void put_ip4(char *r, uint32_t ip) {
  r[0] = AF_INET;
  r[4] = (char)(ip >> 24), r[5] = (char)(ip >> 16), r[6] = (char)(ip >> 8), r[7] = (char)ip;
}

static long if_ioctl(unsigned cmd, uint64_t arg) {
  char r[40];
  int nif = eth0.present ? 2 : 1;
  if (cmd == SIOCGIFCONF) { /* struct ifconf { int len; char *buf; }: addressed interfaces */
    int32_t len;
    uint64_t ubuf;
    if (copy_from_user(&len, (void *)arg, 4) || copy_from_user(&ubuf, (char *)arg + 8, 8))
      return -EFAULT;
    int32_t out = 0;
    for (int i = 1; i <= nif; i++) {
      if (i == 2 && !eth0.configured)
        continue;
      if (ubuf && len >= out + 40) {
        memset(r, 0, sizeof(r));
        strcpy(r, i == 1 ? "lo" : "eth0");
        put_ip4(r + 16, i == 1 ? 0x7f000001U : eth0.addr);
        if (copy_to_user((char *)ubuf + out, r, 40))
          return -EFAULT;
      }
      out += 40;
    }
    return copy_to_user((void *)arg, &out, 4) ? -EFAULT : 0;
  }
  if (copy_from_user(r, (void *)arg, 40))
    return -EFAULT;
  int32_t idx;
  memcpy(&idx, r + 16, 4);
  if (cmd == SIOCGIFNAME) {
    if (idx < 1 || idx > nif)
      return -ENODEV;
    memset(r, 0, 16);
    strcpy(r, idx == 1 ? "lo" : "eth0");
  } else {
    r[15] = 0;
    int i = !strcmp(r, "lo") ? 1 : !strcmp(r, "eth0") && nif == 2 ? 2 : 0;
    if (!i)
      return -ENODEV;
    memset(r + 16, 0, 24);
    if (i == 2 && !eth0.configured && (cmd == SIOCGIFADDR || cmd == SIOCGIFNETMASK || cmd == SIOCGIFBRDADDR))
      return -EADDRNOTAVAIL;
    switch (cmd) {
    case SIOCGIFINDEX: idx = i; memcpy(r + 16, &idx, 4); break;
    case SIOCGIFFLAGS: /* UP, BROADCAST / LOOPBACK, RUNNING, MULTICAST */
      idx = i == 1 ? 0x49 : 0x1043;
      memcpy(r + 16, &idx, 4);
      break;
    case SIOCGIFMTU: idx = i == 1 ? 65536 : 1500; memcpy(r + 16, &idx, 4); break;
    case SIOCGIFADDR: put_ip4(r + 16, i == 1 ? 0x7f000001U : eth0.addr); break;
    case SIOCGIFNETMASK: put_ip4(r + 16, i == 1 ? 0xff000000U : eth0.mask); break;
    case SIOCGIFBRDADDR: put_ip4(r + 16, i == 1 ? 0 : eth0.addr | ~eth0.mask); break;
    case SIOCGIFHWADDR:
      r[16] = i == 1 ? 4 : 1, r[17] = 0; /* ARPHRD_LOOPBACK, ARPHRD_ETHER */
      if (i == 2)
        memcpy(r + 18, eth0.mac, 6);
      break;
    default: return -EINVAL;
    }
  }
  return copy_to_user((void *)arg, r, 40) ? -EFAULT : 0;
}

static long sock_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  struct sock *s = SK(f);
  if (cmd == FIONREAD) {
    int32_t n;
    if (s->type == SOCK_STREAM)
      n = (int32_t)s->rbytes;
    else
      n = list_empty(&s->rq) ? 0 : (int32_t)list_entry(s->rq.next, struct skb, link)->len;
    return copy_to_user((void *)arg, &n, 4) ? -EFAULT : 0;
  }
  if (cmd == SIOCOUTQ || cmd == SIOCATMARK) { /* (no out-of-band data) */
    int32_t n = 0;
    return copy_to_user((void *)arg, &n, 4) ? -EFAULT : 0;
  }
  if ((cmd & 0xff00) == 0x8900)
    return if_ioctl(cmd, arg);
  return -ENOTTY;
}

static const struct file_ops sock_fops = {
    .read = sock_read, .write = sock_write, .poll = sock_poll, .release = sock_release, .ioctl = sock_ioctl};
