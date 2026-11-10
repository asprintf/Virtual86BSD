/* tcp.c - TCP over the network interface (RFC 9293's state machine).

   A connection is a `struct tcb', joined to its socket while there is one
   (s->tcb, t->s).  Received data goes straight into the socket's queue
   (sock_queue), so reading is the socket layer's; what is sent is kept in
   the tcb's 64 KiB buffer until acknowledged.  Retransmission by timeout
   (go back to the first unacknowledged byte), RTO from smoothed RTT
   (RFC 6298, at least 200 ms) with exponential backoff; fast retransmit
   on three duplicate ACKs; slow start and congestion avoidance;
   zero-window probes; MSS option (1460 offered).  Not done: window
   scaling (windows up to 64 KiB), SACK, timestamps, delayed ACKs, urgent
   data.  Segments that come early wait (up to 64 KiB) for the gap.

   Passive opens: a SYN for a listening socket makes a child socket and a
   tcb in SYN_RECEIVED; the handshake's ACK puts the child on the
   listener's backlog, where accept() finds it.  A closed socket's tcb
   lives on ("orphaned") until its FIN is acknowledged and TIME_WAIT is
   over (10 s here; 2 MSL is 4 minutes, Linux uses 60 s). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <sys/socketvar.h>
#include <machine/cpu.h>

enum { CLOSED, SYN_SENT, SYN_RCVD, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT };

#define FIN 1
#define SYN 2
#define RST 4
#define PSH 8
#define ACK 16

#define SBUF (64 * 1024)
#define MSS_OURS 1460
#define TW_MS 10000
#define FW2_MS 60000

#define LT(a, b) ((int32_t)((a) - (b)) < 0)
#define LEQ(a, b) ((int32_t)((a) - (b)) <= 0)
#define GT(a, b) ((int32_t)((a) - (b)) > 0)
#define GEQ(a, b) ((int32_t)((a) - (b)) >= 0)

struct tcb {
  struct list link;
  int state;
  struct sock *s;        /* the socket; NULL when orphaned */
  struct sock *listener; /* SYN_RCVD: whose (a reference) */
  uint32_t lip, rip;
  uint16_t lport, rport;
  uint32_t iss, snd_una, snd_nxt, snd_wnd;
  uint32_t snd_max; /* the highest sequence number sent (snd_nxt goes back on a timeout) */
  int dupacks;
  uint32_t irs, rcv_nxt, rcv_adv;
  uint8_t *sbuf;
  size_t slen; /* bytes from snd_una */
  bool fin_queued, fin_sent, fin_seq_set;
  uint32_t fin_seq;
  uint16_t mss;
  uint32_t cwnd, ssthresh;
  uint64_t rto_at, tw_until;
  uint32_t rto;
  int retries;
  bool rtt_on;
  uint32_t rtt_seq;
  uint64_t rtt_start;
  int32_t srtt, rttvar; /* ms; srtt 0: none yet */
  struct list ooo;      /* segments that came early, by sequence number */
  size_t ooo_bytes;
};

struct ooo {
  struct list link;
  uint32_t seq;
  size_t len;
  uint8_t data[];
};

static struct list tcbs = LIST_INIT(tcbs);

static inline uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static inline void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v; }
static inline void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

/* the window to offer: room in the socket's queue */
static uint32_t rcv_wnd(struct tcb *t) {
  if (!t->s)
    return 0;
  size_t room = SOCK_BUF > t->s->rbytes ? SOCK_BUF - t->s->rbytes : 0;
  return room > 65535 ? 65535 : (uint32_t)room;
}

static void xmit(struct tcb *t, uint32_t seq, int flags, const uint8_t *data, size_t n) {
  uint8_t seg[24 + MSS_OURS];
  size_t hl = (flags & SYN) ? 24 : 20;
  memset(seg, 0, hl);
  put16(seg, t->lport);
  put16(seg + 2, t->rport);
  put32(seg + 4, seq);
  if (flags & ACK)
    put32(seg + 8, t->rcv_nxt);
  seg[12] = (uint8_t)((hl / 4) << 4);
  seg[13] = (uint8_t)flags;
  uint32_t w = rcv_wnd(t);
  put16(seg + 14, (uint16_t)w);
  if (flags & ACK)
    t->rcv_adv = t->rcv_nxt + w;
  if (flags & SYN) {
    seg[20] = 2, seg[21] = 4;
    put16(seg + 22, MSS_OURS);
  }
  if (n)
    memcpy(seg + hl, data, n);
  put16(seg + 16, ip_csum(seg, hl + n, ip_pseudo(t->lip, t->rip, 6, hl + n)));
  ip_output(6, t->lip, t->rip, seg, hl + n);
}

/* a reset answering segment seg (from src to dst) that has no connection */
static void reset_for(uint32_t src, uint32_t dst, const uint8_t *seg, size_t dlen) {
  int flags = seg[13];
  uint8_t r[20];
  memset(r, 0, 20);
  put16(r, be16(seg + 2));
  put16(r + 2, be16(seg));
  if (flags & ACK) {
    put32(r + 4, be32(seg + 8));
    r[13] = RST;
  } else {
    put32(r + 8, be32(seg + 4) + (uint32_t)dlen + ((flags & SYN) ? 1 : 0) + ((flags & FIN) ? 1 : 0));
    r[13] = RST | ACK;
  }
  r[12] = 5 << 4;
  put16(r + 16, ip_csum(r, 20, ip_pseudo(dst, src, 6, 20)));
  ip_output(6, dst, src, r, 20);
}

static void arm(struct tcb *t) {
  if (t->snd_una != t->snd_max || t->state == SYN_SENT || t->state == SYN_RCVD || (t->slen && !t->snd_wnd)) {
    if (!t->rto_at)
      t->rto_at = net_now_ms() + t->rto;
  } else
    t->rto_at = 0;
  net_wake(); /* (the thread recomputes its deadline) */
}

static void tcb_free(struct tcb *t) {
  list_del(&t->link);
  if (t->s) {
    t->s->tcb = NULL;
    t->s = NULL;
  }
  if (t->listener)
    sock_put(t->listener);
  list_for_each_safe(p, n, &t->ooo) {
    list_del(p);
    kfree(list_entry(p, struct ooo, link));
  }
  kfree(t->sbuf);
  kfree(t);
}

/* the connection ends with error err (0: closed in order) */
static void tcb_abort(struct tcb *t, int err) {
  struct sock *s = t->s;
  if (s && t->state == SYN_SENT) {
    tcb_free(t);
    connect_failed(s);
    s->error = err ? err : ECONNREFUSED;
    sock_wake(s);
    return;
  }
  if (s) {
    if (err)
      s->error = err;
    s->state = SS_DISCONNECTED;
    s->shut_rd = s->shut_wr = true;
    sock_wake(s);
  }
  tcb_free(t);
}

static struct tcb *tcb_new(void) {
  struct tcb *t = kzalloc(sizeof(*t));
  if (!t)
    return NULL;
  t->sbuf = kmalloc(SBUF);
  if (!t->sbuf) {
    kfree(t);
    return NULL;
  }
  uint32_t r;
  get_random_bytes(&r, sizeof(r));
  t->iss = r;
  t->snd_una = t->iss;
  t->snd_nxt = t->snd_max = t->iss + 1; /* the SYN */
  t->mss = 536;
  t->rto = 1000;
  t->cwnd = 10 * 536;
  t->ssthresh = 65535;
  list_init(&t->ooo);
  list_add_tail(&tcbs, &t->link);
  return t;
}

/* send what the windows allow, and a FIN when everything is sent */
static void output(struct tcb *t) {
  if (t->state < ESTABLISHED || t->state == TIME_WAIT)
    return;
  uint32_t wnd = MIN(t->snd_wnd, t->cwnd);
  for (;;) {
    size_t sent = MIN((size_t)(t->snd_nxt - t->snd_una), t->slen);
    size_t avail = t->slen - sent;
    int32_t can = (int32_t)(t->snd_una + wnd - t->snd_nxt);
    if (!avail || can <= 0 || t->fin_sent)
      break;
    size_t n = MIN(avail, MIN((size_t)can, (size_t)t->mss));
    if (!t->rtt_on) {
      t->rtt_on = true;
      t->rtt_seq = t->snd_nxt + (uint32_t)n;
      t->rtt_start = net_now_ms();
    }
    xmit(t, t->snd_nxt, ACK | PSH, t->sbuf + sent, n);
    t->snd_nxt += (uint32_t)n;
  }
  if (t->fin_queued && !t->fin_sent && (size_t)(t->snd_nxt - t->snd_una) == t->slen) {
    t->fin_seq = t->snd_nxt;
    t->fin_seq_set = true;
    xmit(t, t->snd_nxt, FIN | ACK, NULL, 0);
    t->snd_nxt++;
    t->fin_sent = true;
  }
  if (GT(t->snd_nxt, t->snd_max))
    t->snd_max = t->snd_nxt;
  arm(t);
}

/* keep a segment that came early (seq after rcv_nxt) */
static void ooo_add(struct tcb *t, uint32_t seq, const uint8_t *d, size_t n) {
  if (t->ooo_bytes + n > 65536)
    return;
  struct list *at = &t->ooo;
  list_for_each(p, &t->ooo) {
    struct ooo *o = list_entry(p, struct ooo, link);
    if (o->seq == seq && o->len >= n)
      return; /* had it */
    if (GT(o->seq, seq)) {
      at = p;
      break;
    }
  }
  struct ooo *o = kmalloc(sizeof(*o) + n);
  if (!o)
    return;
  o->seq = seq;
  o->len = n;
  memcpy(o->data, d, n);
  list_add_tail(at, &o->link); /* (before `at') */
  t->ooo_bytes += n;
}

/* what waited and now follows rcv_nxt goes to the socket */
static void ooo_drain(struct tcb *t) {
  list_for_each_safe(p, nx, &t->ooo) {
    struct ooo *o = list_entry(p, struct ooo, link);
    if (GT(o->seq, t->rcv_nxt))
      break;
    if (GT(o->seq + (uint32_t)o->len, t->rcv_nxt) && t->s) {
      size_t skip = t->rcv_nxt - o->seq;
      if (sock_queue(t->s, o->data + skip, o->len - skip))
        t->rcv_nxt += (uint32_t)(o->len - skip);
      else
        break; /* no room: it waits */
    }
    list_del(p);
    t->ooo_bytes -= o->len;
    kfree(o);
  }
}

static void established(struct tcb *t) {
  struct sock *s = t->s;
  t->rto_at = 0;
  t->retries = 0;
  if (s) {
    s->state = SS_CONNECTED;
    sock_wake(s);
  }
}

/* ---- from the socket layer ---- */

int tcp_connect(struct sock *s, const struct inaddr *src, const struct inaddr *dst, bool nonblock) {
  struct tcb *t = tcb_new();
  if (!t)
    return -ENOBUFS;
  t->s = s;
  s->tcb = t;
  t->lip = in_ip4(src), t->lport = src->port;
  t->rip = in_ip4(dst), t->rport = dst->port;
  t->state = SYN_SENT;
  xmit(t, t->iss, SYN, NULL, 0);
  arm(t);
  return tcp_connect_wait(s, nonblock);
}

int tcp_connect_wait(struct sock *s, bool nonblock) {
  if (nonblock)
    return -EINPROGRESS;
  uint64_t f = irq_save();
  while (s->tcb && s->tcb->state == SYN_SENT) {
    if (sleep_on(&s->wq, 0, true)) {
      irq_restore(f);
      return -EINTR; /* (the connection goes on: POSIX) */
    }
  }
  irq_restore(f);
  if (s->tcb && s->tcb->state >= ESTABLISHED)
    return 0;
  int e = s->error ? s->error : ECONNREFUSED;
  s->error = 0;
  return -e;
}

long tcp_send(struct sock *s, const char *ubuf, size_t n, bool nonblock, int flags) {
  size_t done = 0;
  uint64_t dl = s->sndtimeo ? sat_add(monotonic_ns(), s->sndtimeo) : 0;
  while (done < n) {
    struct tcb *t = s->tcb;
    if (s->error && !done) {
      int e = s->error;
      s->error = 0;
      return -e;
    }
    if (!t || s->shut_wr || (t->state != ESTABLISHED && t->state != CLOSE_WAIT)) {
      if (done)
        break;
      if (!(flags & 0x4000)) /* MSG_NOSIGNAL */
        signal_simple(cur_proc(), SIGPIPE);
      return -EPIPE;
    }
    size_t room = SBUF - t->slen;
    if (!room) {
      if (nonblock)
        return done ? (long)done : -EAGAIN;
      uint64_t f = irq_save();
      int r = 0;
      if (s->tcb == t && t->slen == SBUF)
        r = sleep_on(&s->wq, dl, true);
      irq_restore(f);
      if (r)
        return done ? (long)done : r == -ETIMEDOUT ? -EAGAIN : -ERESTARTSYS;
      continue;
    }
    /* through a buffer of our own: copying from the user may sleep, and
       acknowledgements meanwhile move the send buffer */
    size_t c = MIN(room, n - done);
    if (c > 16384)
      c = 16384;
    char *k = kmalloc(c);
    if (!k)
      return done ? (long)done : -ENOBUFS;
    if (copy_from_user(k, ubuf + done, c)) {
      kfree(k);
      return done ? (long)done : -EFAULT;
    }
    t = s->tcb;
    if (!t || t->slen + c > SBUF) {
      kfree(k);
      continue;
    }
    memcpy(t->sbuf + t->slen, k, c);
    kfree(k);
    t->slen += c;
    done += c;
    output(t);
  }
  return (long)done;
}

bool tcp_writable(struct sock *s) {
  struct tcb *t = s->tcb;
  return t && (t->state == ESTABLISHED || t->state == CLOSE_WAIT) && t->slen < SBUF && !s->shut_wr;
}

void tcp_shutdown_wr(struct sock *s) {
  struct tcb *t = s->tcb;
  if (!t || t->fin_queued)
    return;
  if (t->state == SYN_SENT || t->state == SYN_RCVD) {
    tcb_abort(t, 0);
    return;
  }
  t->fin_queued = true;
  if (t->state == ESTABLISHED)
    t->state = FIN_WAIT_1;
  else if (t->state == CLOSE_WAIT)
    t->state = LAST_ACK;
  output(t);
}

void tcp_close(struct sock *s) {
  struct tcb *t = s->tcb;
  if (!t)
    return;
  bool reset = (s->linger[0] && !s->linger[1]) || s->rbytes; /* unread data: reset (as Linux) */
  s->tcb = NULL;
  t->s = NULL;
  if (t->state == SYN_SENT) {
    tcb_free(t);
    return;
  }
  if (reset && t->state != TIME_WAIT) {
    xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
    tcb_free(t);
    return;
  }
  if (!t->fin_queued) {
    t->fin_queued = true;
    if (t->state == ESTABLISHED || t->state == SYN_RCVD)
      t->state = FIN_WAIT_1;
    else if (t->state == CLOSE_WAIT)
      t->state = LAST_ACK;
    output(t);
  }
}

/* a listener closes: connections still in the handshake go */
void tcp_listener_close(struct sock *l) {
  list_for_each_safe(p, n, &tcbs) {
    struct tcb *t = list_entry(p, struct tcb, link);
    if (t->listener == l && t->state == SYN_RCVD) {
      xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
      struct sock *c = t->s;
      tcb_free(t);
      if (c)
        sock_put(c);
    }
  }
}

void tcp_read_done(struct sock *s) {
  struct tcb *t = s->tcb;
  if (!t || t->state < ESTABLISHED)
    return;
  /* a window update when the window opened by a segment or more */
  uint32_t w = rcv_wnd(t);
  if ((int32_t)(t->rcv_nxt + w - t->rcv_adv) >= (int32_t)MIN((uint32_t)MSS_OURS * 2, 32768U))
    xmit(t, t->snd_nxt, ACK, NULL, 0);
}

/* ---- from the network ---- */

static struct tcb *lookup(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport) {
  list_for_each(p, &tcbs) {
    struct tcb *t = list_entry(p, struct tcb, link);
    if (t->lport == lport && t->rport == rport && t->lip == lip && t->rip == rip)
      return t;
  }
  return NULL;
}

static uint16_t parse_mss(const uint8_t *seg, size_t hl) {
  for (size_t i = 20; i < hl;) {
    uint8_t k = seg[i];
    if (k == 0)
      break;
    if (k == 1) {
      i++;
      continue;
    }
    if (i + 1 >= hl || seg[i + 1] < 2)
      break;
    if (k == 2 && seg[i + 1] == 4 && i + 4 <= hl)
      return be16(seg + i + 2);
    i += seg[i + 1];
  }
  return 536;
}

static void rtt_sample(struct tcb *t, uint32_t ms) {
  if (!t->srtt) {
    t->srtt = (int32_t)ms ? (int32_t)ms : 1;
    t->rttvar = (int32_t)ms / 2;
  } else {
    int32_t d = (int32_t)ms - t->srtt;
    t->rttvar += ((d < 0 ? -d : d) - t->rttvar) / 4;
    t->srtt += d / 8;
    if (t->srtt < 1)
      t->srtt = 1;
  }
  uint32_t rto = (uint32_t)(t->srtt + MAX(4 * t->rttvar, 10));
  t->rto = rto < 200 ? 200 : rto > 60000 ? 60000 : rto;
}

static void passive_open(struct sock *l, uint32_t src, uint32_t dst, const uint8_t *seg, size_t hl) {
  if (sock_backlog_full(l))
    return; /* dropped: the peer will try again */
  int pending = 0;
  list_for_each(p, &tcbs) pending += list_entry(p, struct tcb, link)->listener == l;
  if (pending >= 64)
    return;
  struct sock *c = sock_new(SOCK_STREAM);
  if (!c)
    return;
  struct tcb *t = tcb_new();
  if (!t) {
    sock_put(c);
    return;
  }
  c->family = l->family;
  c->proto = 6;
  c->nodelay = l->nodelay;
  in_set4(&c->la, dst, be16(seg + 2));
  in_set4(&c->ra, src, be16(seg));
  c->has_ra = true;
  c->tcb = t;
  t->s = c; /* (the reference sock_new gave: the backlog's, later) */
  t->listener = l;
  l->ref++;
  t->lip = dst, t->lport = be16(seg + 2);
  t->rip = src, t->rport = be16(seg);
  t->irs = be32(seg + 4);
  t->rcv_nxt = t->irs + 1;
  t->snd_wnd = be16(seg + 14);
  t->mss = MIN(parse_mss(seg, hl), (uint16_t)MSS_OURS);
  t->cwnd = 10U * t->mss;
  t->state = SYN_RCVD;
  xmit(t, t->iss, SYN | ACK, NULL, 0);
  arm(t);
}

void tcp_input(uint32_t src, uint32_t dst, const uint8_t *seg, size_t len) {
  if (len < 20)
    return;
  size_t hl = (size_t)(seg[12] >> 4) * 4;
  if (hl < 20 || hl > len || ip_csum(seg, len, ip_pseudo(src, dst, 6, len)) != 0)
    return;
  int flags = seg[13];
  uint32_t seq = be32(seg + 4), ack = be32(seg + 8);
  uint16_t win = be16(seg + 14);
  const uint8_t *data = seg + hl;
  size_t dlen = len - hl;
  struct tcb *t = lookup(dst, be16(seg + 2), src, be16(seg));
  if (!t) {
    if (flags & RST)
      return;
    if ((flags & SYN) && !(flags & ACK)) {
      struct inaddr d, s;
      in_set4(&d, dst, be16(seg + 2));
      in_set4(&s, src, be16(seg));
      struct sock *l = in_find(6, &d, &s);
      if (l) {
        passive_open(l, src, dst, seg, hl);
        sock_put(l);
        return;
      }
    }
    reset_for(src, dst, seg, dlen);
    return;
  }

  if (t->state == SYN_SENT) {
    bool ackok = (flags & ACK) && GT(ack, t->iss) && LEQ(ack, t->snd_nxt);
    if ((flags & ACK) && !ackok) {
      if (!(flags & RST))
        reset_for(src, dst, seg, dlen);
      return;
    }
    if (flags & RST) {
      if (ackok)
        tcb_abort(t, ECONNREFUSED);
      return;
    }
    if (!(flags & SYN))
      return;
    t->irs = seq;
    t->rcv_nxt = seq + 1;
    t->mss = MIN(parse_mss(seg, hl), (uint16_t)MSS_OURS);
    t->cwnd = 10U * t->mss;
    t->snd_wnd = win;
    if (ackok) {
      t->snd_una = ack;
      t->state = ESTABLISHED;
      xmit(t, t->snd_nxt, ACK, NULL, 0);
      established(t);
      output(t);
    } else { /* simultaneous open */
      t->state = SYN_RCVD;
      xmit(t, t->iss, SYN | ACK, NULL, 0);
    }
    return;
  }

  /* is the segment in the window? */
  uint32_t w = rcv_wnd(t);
  size_t seglen = dlen + ((flags & SYN) ? 1 : 0) + ((flags & FIN) ? 1 : 0);
  bool ok;
  if (seglen == 0)
    ok = w == 0 ? seq == t->rcv_nxt : GEQ(seq, t->rcv_nxt) && LT(seq, t->rcv_nxt + w);
  else
    ok = w == 0 ? false
                : (GEQ(seq, t->rcv_nxt) && LT(seq, t->rcv_nxt + w)) ||
                      (GEQ(seq + (uint32_t)seglen - 1, t->rcv_nxt) && LT(seq + (uint32_t)seglen - 1, t->rcv_nxt + w));
  if (!ok && w == 0 && seglen && seq == t->rcv_nxt && !(flags & (SYN | RST)))
    ok = true; /* (a window probe: its ACK and FIN still count) */
  if (!ok) {
    if (!(flags & RST))
      xmit(t, t->snd_nxt, ACK, NULL, 0);
    return;
  }
  if (flags & RST) {
    if (t->state == SYN_RCVD) {
      struct sock *c = t->s;
      tcb_free(t);
      if (c)
        sock_put(c);
    } else if (t->state >= CLOSING)
      tcb_free(t);
    else
      tcb_abort(t, ECONNRESET);
    return;
  }
  if (flags & SYN) {
    xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
    tcb_abort(t, ECONNRESET);
    return;
  }
  if (!(flags & ACK))
    return;

  if (t->state == SYN_RCVD) {
    if (!(GT(ack, t->snd_una) && LEQ(ack, t->snd_nxt))) {
      reset_for(src, dst, seg, dlen);
      return;
    }
    t->snd_una = ack;
    t->snd_wnd = win;
    t->state = ESTABLISHED;
    struct sock *l = t->listener, *c = t->s;
    t->listener = NULL;
    established(t);
    if (l && c && l->state == SS_LISTENING && !sock_backlog_full(l))
      sock_add_backlog(l, c);
    else {
      xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
      tcb_free(t);
      if (c)
        sock_put(c);
      if (l)
        sock_put(l);
      return;
    }
    sock_put(l);
  }

  if (GT(ack, t->snd_max)) { /* acknowledges what was never sent */
    xmit(t, t->snd_nxt, ACK, NULL, 0);
    return;
  }
  if (GT(ack, t->snd_nxt)) /* sent before going back after a timeout, and arrived */
    t->snd_nxt = ack;
  if (ack == t->snd_una && !dlen && win == t->snd_wnd && t->snd_una != t->snd_max && !(flags & FIN)) {
    /* a duplicate ACK: the third one resends the first segment at once */
    if (++t->dupacks == 3 && t->slen) {
      uint32_t flight = t->snd_max - t->snd_una;
      t->ssthresh = MAX(flight / 2, 2U * t->mss);
      t->cwnd = t->ssthresh;
      xmit(t, t->snd_una, ACK | PSH, t->sbuf, MIN(t->slen, (size_t)t->mss));
      t->rtt_on = false;
    }
  } else if (GT(ack, t->snd_una))
    t->dupacks = 0;
  if (GT(ack, t->snd_una)) {
    uint32_t acked = ack - t->snd_una;
    size_t d = MIN((size_t)acked, t->slen);
    if (d) {
      memmove(t->sbuf, t->sbuf + d, t->slen - d);
      t->slen -= d;
    }
    t->snd_una = ack;
    if (t->rtt_on && GEQ(ack, t->rtt_seq)) {
      rtt_sample(t, (uint32_t)(net_now_ms() - t->rtt_start));
      t->rtt_on = false;
    }
    if (t->cwnd < t->ssthresh)
      t->cwnd += t->mss;
    else
      t->cwnd += MAX(1U, (uint32_t)t->mss * t->mss / t->cwnd);
    t->retries = 0;
    t->rto_at = 0;
    if (t->s)
      sock_wake(t->s);
  }
  t->snd_wnd = win;
  if (t->fin_seq_set && GT(t->snd_una, t->fin_seq)) { /* our FIN is acknowledged */
    t->fin_sent = true;
    if (t->state == FIN_WAIT_1)
      t->state = FIN_WAIT_2, t->tw_until = t->s ? 0 : net_now_ms() + FW2_MS;
    else if (t->state == CLOSING)
      t->state = TIME_WAIT, t->tw_until = net_now_ms() + TW_MS;
    else if (t->state == LAST_ACK) {
      tcb_abort(t, 0);
      return;
    }
  }

  bool ack_now = false;
  uint32_t fseq = seq + (uint32_t)dlen; /* where a FIN would be */
  if (dlen && (t->state == ESTABLISHED || t->state == FIN_WAIT_1 || t->state == FIN_WAIT_2)) {
    if (LEQ(seq, t->rcv_nxt) && GT(seq + (uint32_t)dlen, t->rcv_nxt)) {
      size_t skip = t->rcv_nxt - seq;
      size_t n = MIN(dlen - skip, (size_t)w);
      if (!t->s) { /* nobody reads any more */
        xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
        tcb_free(t);
        return;
      }
      if (n && sock_queue(t->s, data + skip, n))
        t->rcv_nxt += (uint32_t)n;
      ooo_drain(t);
    } else if (GT(seq, t->rcv_nxt) && t->s)
      ooo_add(t, seq, data, MIN(dlen, (size_t)w));
    ack_now = true;
  }
  if ((flags & FIN) && fseq == t->rcv_nxt) {
    t->rcv_nxt++;
    ack_now = true;
    if (t->s) {
      t->s->shut_rd = true;
      sock_wake(t->s);
    }
    switch (t->state) {
    case ESTABLISHED: t->state = CLOSE_WAIT; break;
    case FIN_WAIT_1:
      t->state = (t->fin_seq_set && GT(t->snd_una, t->fin_seq)) ? TIME_WAIT : CLOSING;
      if (t->state == TIME_WAIT)
        t->tw_until = net_now_ms() + TW_MS;
      break;
    case FIN_WAIT_2: t->state = TIME_WAIT, t->tw_until = net_now_ms() + TW_MS; break;
    case TIME_WAIT: t->tw_until = net_now_ms() + TW_MS; break;
    }
  }
  if (ack_now)
    xmit(t, t->snd_nxt, ACK, NULL, 0);
  output(t);
}

void tcp_icmp_error(uint32_t src, uint32_t dst, const uint8_t *hdr, size_t n, int err) {
  if (n < 8)
    return;
  struct tcb *t = lookup(src, be16(hdr), dst, be16(hdr + 2));
  if (!t)
    return;
  if (t->state == SYN_SENT && err == ECONNREFUSED)
    tcb_abort(t, err); /* (port unreachable for TCP is unusual; others are soft) */
  else if (t->s)
    t->s->error = t->state == SYN_SENT ? err : t->s->error;
}

uint64_t tcp_timers(uint64_t now) {
  uint64_t next = 0;
  list_for_each_safe(p, nx, &tcbs) {
    struct tcb *t = list_entry(p, struct tcb, link);
    if (t->tw_until) {
      if (now >= t->tw_until && (t->state == TIME_WAIT || (t->state == FIN_WAIT_2 && !t->s))) {
        tcb_free(t);
        continue;
      }
      if (!next || t->tw_until < next)
        next = t->tw_until;
    }
    if (!t->rto_at)
      continue;
    if (now >= t->rto_at) {
      t->rto_at = 0;
      t->rtt_on = false;
      int max = t->state == SYN_SENT ? 6 : t->state == SYN_RCVD ? 5 : 12;
      if (++t->retries > max) {
        if (t->state == SYN_RCVD) {
          struct sock *c = t->s;
          tcb_free(t);
          if (c)
            sock_put(c);
        } else {
          if (t->state != SYN_SENT)
            xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
          tcb_abort(t, ETIMEDOUT);
        }
        continue;
      }
      t->rto = MIN(t->rto * 2, 60000U);
      if (t->state == SYN_SENT)
        xmit(t, t->iss, SYN, NULL, 0);
      else if (t->state == SYN_RCVD)
        xmit(t, t->iss, SYN | ACK, NULL, 0);
      else if (t->snd_una == t->snd_max && t->slen && !t->snd_wnd) {
        xmit(t, t->snd_una, ACK, t->sbuf, 1); /* a probe of the zero window */
        t->retries = 0;
      } else {
        uint32_t flight = t->snd_max - t->snd_una;
        t->ssthresh = MAX(flight / 2, 2U * t->mss);
        t->cwnd = t->mss;
        t->snd_nxt = t->snd_una; /* go back */
        if (t->fin_sent && GEQ(t->fin_seq, t->snd_una))
          t->fin_sent = false;
        output(t);
      }
      arm(t);
    }
    if (t->rto_at && (!next || t->rto_at < next))
      next = t->rto_at;
  }
  return next;
}
