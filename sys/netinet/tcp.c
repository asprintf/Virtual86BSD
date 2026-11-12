/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
   listener's backlog, where accept() finds it.  When a listener has 16
   handshakes going, a SYN is answered without keeping anything (a SYN
   cookie: the sequence number of the SYN-ACK says whose it is, and the
   ACK that comes back makes the connection), so that a flood of SYNs from
   addresses that do not exist cannot fill the listener up.

   Hardening: the initial sequence numbers are a keyed hash of the
   connection plus a clock (RFC 6528); a RST is accepted only at exactly the
   next sequence number, and a RST or SYN inside the window but elsewhere
   gets one ACK back, rate limited (RFC 5961); an ACK that acknowledges
   nothing that was sent, or is more than a window old, is answered the
   same way; the window offered by a segment counts only when the segment is
   newer than the last that set it; an MSS under 216 is taken as 216; the
   send buffer is allocated when the connection is established, not for a
   SYN; what waits for a gap is limited in segments as well as in bytes;
   an ICMP error is believed only when it names a sequence number that is in
   flight; the RSTs for segments that have no connection are limited per
   second (net.inet.tcp.rstppslimit); the connections of closed sockets do
   not wait for a peer that keeps its window shut for ever.  A closed socket's tcb
   lives on ("orphaned") until its FIN is acknowledged and TIME_WAIT is
   over (10 s here; 2 MSL is 4 minutes, Linux uses 60 s). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
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
#define TCP_MINMSS 216
#define MAXWIN 65535       /* the largest window there is (no window scaling) */
#define SYN_COOKIE_AT 16   /* handshakes in progress per listener before SYN cookies */
#define OOO_MAX 32         /* segments waiting for a gap, per connection */
#define CHALLENGE_PPS 100  /* challenge ACKs per second, all connections */

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
  uint32_t snd_wl1, snd_wl2; /* the segment (its sequence and ACK numbers) that set snd_wnd */
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
  int nooo;
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

/* ---- SipHash-2-4 (Aumasson and Bernstein), the keyed hash behind the
   initial sequence numbers and the SYN cookies ---- */

static inline uint64_t rotl64(uint64_t x, int b) { return x << b | x >> (64 - b); }

#define SIPROUND \
  do { \
    v0 += v1, v1 = rotl64(v1, 13), v1 ^= v0, v0 = rotl64(v0, 32); \
    v2 += v3, v3 = rotl64(v3, 16), v3 ^= v2; \
    v0 += v3, v3 = rotl64(v3, 21), v3 ^= v0; \
    v2 += v1, v1 = rotl64(v1, 17), v1 ^= v2, v2 = rotl64(v2, 32); \
  } while (0)

static uint64_t siphash24(const uint64_t k[2], const uint8_t *m, size_t n) {
  uint64_t v0 = 0x736f6d6570736575ULL ^ k[0], v1 = 0x646f72616e646f6dULL ^ k[1];
  uint64_t v2 = 0x6c7967656e657261ULL ^ k[0], v3 = 0x7465646279746573ULL ^ k[1];
  uint64_t b = (uint64_t)n << 56, w;
  size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    w = 0;
    for (int j = 7; j >= 0; j--)
      w = w << 8 | m[i + (size_t)j];
    v3 ^= w;
    SIPROUND; SIPROUND;
    v0 ^= w;
  }
  for (size_t j = 0; i + j < n; j++)
    b |= (uint64_t)m[i + j] << (8 * j);
  v3 ^= b;
  SIPROUND; SIPROUND;
  v0 ^= b;
  v2 ^= 0xff;
  SIPROUND; SIPROUND; SIPROUND; SIPROUND;
  return v0 ^ v1 ^ v2 ^ v3;
}

static uint64_t isn_key[2], cookie_key[2];
static bool keys_ready;

static void keys_init(void) {
  if (keys_ready)
    return;
  get_random_bytes(isn_key, sizeof isn_key);
  get_random_bytes(cookie_key, sizeof cookie_key);
  keys_ready = true;
}

/* RFC 6528: the initial sequence number of a connection is a keyed hash of
   its four addresses and ports plus a clock of 4 microsecond ticks, so that
   it cannot be guessed, and a new connection on the same four never starts
   behind the old one */
static uint32_t tcp_isn(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport) {
  uint8_t m[12];
  keys_init();
  put32(m, lip), put32(m + 4, rip), put16(m + 8, lport), put16(m + 10, rport);
  return (uint32_t)siphash24(isn_key, m, sizeof m) + (uint32_t)(monotonic_ns() / 4000);
}

/* SYN cookies: the sequence number of a SYN-ACK that keeps no state is
   5 bits of time (64 second steps), 3 of the MSS (an index into a table) and
   24 of a keyed hash of the connection, the time and the MSS.  The ACK that
   completes the handshake gives it back, plus one. */
static const uint16_t cookie_mss[] = {TCP_MINMSS, 536, 1024, 1360, 1440, MSS_OURS};
#define NCOOKIE_MSS ((unsigned)(sizeof cookie_mss / sizeof cookie_mss[0]))

static uint32_t cookie_mac(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport, uint32_t irs, unsigned counter,
                           unsigned idx) {
  uint8_t m[20];
  keys_init();
  put32(m, lip), put32(m + 4, rip), put16(m + 8, lport), put16(m + 10, rport), put32(m + 12, irs);
  put32(m + 16, counter << 8 | idx);
  return (uint32_t)siphash24(cookie_key, m, sizeof m) & 0xffffff;
}

static uint32_t cookie_make(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport, uint32_t irs, uint16_t mss) {
  unsigned idx = 0, counter = (unsigned)(net_now_ms() / 64000) & 31;
  for (unsigned i = 0; i < NCOOKIE_MSS; i++)
    if (cookie_mss[i] <= mss)
      idx = i;
  return counter << 27 | idx << 24 | cookie_mac(lip, lport, rip, rport, irs, counter, idx);
}

/* the MSS a cookie stands for, or 0: it is not one made for this connection
   in the last two minutes or so */
static uint16_t cookie_check(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport, uint32_t irs, uint32_t c) {
  unsigned counter = c >> 27, idx = (c >> 24) & 7, now = (unsigned)(net_now_ms() / 64000) & 31;
  if (idx >= NCOOKIE_MSS || ((now - counter) & 31) > 1)
    return 0;
  if ((c & 0xffffff) != cookie_mac(lip, lport, rip, rport, irs, counter, idx))
    return 0;
  return cookie_mss[idx];
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
  static struct ratelim rl;
  if (!net_ratelimit(&rl, tcp_rstppslimit))
    return;
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
  if (t->listener) {
    t->listener->nsyn--;
    sock_put(t->listener);
  }
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
  t->mss = 536;
  t->rto = 1000;
  t->cwnd = 10 * 536;
  t->ssthresh = 65535;
  list_init(&t->ooo);
  list_add_tail(&tcbs, &t->link);
  return t;
}

/* the sequence number of our SYN */
static void tcb_iss(struct tcb *t, uint32_t iss) {
  t->iss = iss;
  t->snd_una = iss;
  t->snd_nxt = t->snd_max = iss + 1;
}

/* the send buffer, when the connection is established (a SYN that never
   completes costs no 64 KiB) */
static bool sbuf_get(struct tcb *t) {
  if (!t->sbuf)
    t->sbuf = kmalloc(SBUF);
  return t->sbuf != NULL;
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
  if (!n || t->ooo_bytes + n > 65536 || t->nooo >= OOO_MAX)
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
  t->nooo++;
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
    t->nooo--;
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

static struct tcb *lookup(uint32_t lip, uint16_t lport, uint32_t rip, uint16_t rport);

int tcp_connect(struct sock *s, const struct inaddr *src, const struct inaddr *dst, bool nonblock) {
  uint32_t rip = in_ip4(dst);
  if (rip >= 0xe0000000U || ip_is_bcast(rip)) /* (a group or a broadcast address is no host) */
    return -ENETUNREACH;
  struct tcb *old = lookup(in_ip4(src), src->port, rip, dst->port);
  if (old) { /* the connection of the same four that has not gone yet: only a finished one is replaced */
    if (old->s || (old->state != TIME_WAIT && old->state != FIN_WAIT_2))
      return -EADDRINUSE;
    tcb_free(old);
  }
  struct tcb *t = tcb_new();
  if (!t)
    return -ENOBUFS;
  t->s = s;
  s->tcb = t;
  t->lip = in_ip4(src), t->lport = src->port;
  t->rip = rip, t->rport = dst->port;
  tcb_iss(t, tcp_isn(t->lip, t->lport, t->rip, t->rport));
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
  } else if (t->state == FIN_WAIT_2 && !t->tw_until) { /* (shutdown() sent the FIN, and now nobody waits for the peer's) */
    t->tw_until = net_now_ms() + FW2_MS;
    net_wake();
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
    if (k == 2 && seg[i + 1] == 4 && i + 4 <= hl) {
      uint16_t m = be16(seg + i + 2);
      return m < TCP_MINMSS ? TCP_MINMSS : m; /* (an MSS of 0 would never send a byte) */
    }
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

/* a connection in SYN_RECEIVED for the listener l, with its socket: from a
   SYN (the sequence number of which is irs) or a cookie */
static struct tcb *child_new(struct sock *l, uint32_t src, uint32_t dst, uint16_t sport, uint16_t dport, uint32_t irs,
                             uint16_t mss) {
  struct sock *c = sock_new(SOCK_STREAM);
  if (!c)
    return NULL;
  struct tcb *t = tcb_new();
  if (!t) {
    sock_put(c);
    return NULL;
  }
  c->family = l->family;
  c->proto = 6;
  c->nodelay = l->nodelay;
  in_set4(&c->la, dst, dport);
  in_set4(&c->ra, src, sport);
  c->has_ra = true;
  c->tcb = t;
  t->s = c; /* (the reference sock_new gave: the backlog's, later) */
  t->listener = l;
  l->ref++;
  l->nsyn++;
  t->lip = dst, t->lport = dport;
  t->rip = src, t->rport = sport;
  t->irs = irs;
  t->rcv_nxt = irs + 1;
  t->mss = mss;
  t->cwnd = 10U * mss;
  t->state = SYN_RCVD;
  return t;
}

/* a SYN-ACK that keeps nothing: its sequence number is the cookie */
static void cookie_synack(uint32_t src, uint32_t dst, const uint8_t *seg, uint16_t mss) {
  uint8_t r[24];
  uint32_t irs = be32(seg + 4);
  memset(r, 0, sizeof r);
  put16(r, be16(seg + 2));
  put16(r + 2, be16(seg));
  put32(r + 4, cookie_make(dst, be16(seg + 2), src, be16(seg), irs, mss));
  put32(r + 8, irs + 1);
  r[12] = 6 << 4;
  r[13] = SYN | ACK;
  put16(r + 14, MAXWIN);
  r[20] = 2, r[21] = 4;
  put16(r + 22, MSS_OURS);
  put16(r + 16, ip_csum(r, 24, ip_pseudo(dst, src, 6, 24)));
  ip_output(6, dst, src, r, 24);
}

static void passive_open(struct sock *l, uint32_t src, uint32_t dst, const uint8_t *seg, size_t hl) {
  if (sock_backlog_full(l))
    return; /* dropped: the peer will try again */
  uint16_t mss = MIN(parse_mss(seg, hl), (uint16_t)MSS_OURS);
  if (l->nsyn >= SYN_COOKIE_AT) { /* (many are going: keep nothing) */
    cookie_synack(src, dst, seg, mss);
    return;
  }
  struct tcb *t = child_new(l, src, dst, be16(seg), be16(seg + 2), be32(seg + 4), mss);
  if (!t)
    return;
  tcb_iss(t, tcp_isn(t->lip, t->lport, t->rip, t->rport));
  t->snd_wnd = be16(seg + 14);
  t->snd_wl1 = t->irs;
  t->snd_wl2 = t->iss;
  xmit(t, t->iss, SYN | ACK, NULL, 0);
  arm(t);
}

/* the ACK that completes a handshake whose SYN was answered with a cookie:
   the connection is made now, in SYN_RECEIVED, and the segment goes on
   through the usual way (it may carry data) */
static struct tcb *cookie_open(struct sock *l, uint32_t src, uint32_t dst, const uint8_t *seg) {
  uint32_t seq = be32(seg + 4), ack = be32(seg + 8);
  uint16_t sport = be16(seg), dport = be16(seg + 2);
  uint16_t mss = cookie_check(dst, dport, src, sport, seq - 1, ack - 1);
  if (!mss)
    return NULL;
  struct tcb *t = child_new(l, src, dst, sport, dport, seq - 1, mss);
  if (!t)
    return NULL;
  tcb_iss(t, ack - 1);
  t->snd_wl1 = t->irs;
  t->snd_wl2 = t->iss;
  return t;
}

/* an ACK for a segment that is not acceptable (RFC 5961): one back, not more
   than so many a second whoever asks */
static void challenge_ack(struct tcb *t) {
  static struct ratelim rl;
  if (net_ratelimit(&rl, CHALLENGE_PPS))
    xmit(t, t->snd_nxt, ACK, NULL, 0);
}

void tcp_input(uint32_t src, uint32_t dst, const uint8_t *seg, size_t len) {
  if (len < 20)
    return;
  size_t hl = (size_t)(seg[12] >> 4) * 4;
  if (hl < 20 || hl > len || ip_csum(seg, len, ip_pseudo(src, dst, 6, len)) != 0)
    return;
  int flags = seg[13];
  uint16_t sport = be16(seg), dport = be16(seg + 2);
  if (!sport || !dport || ((flags & SYN) && (flags & (FIN | RST))))
    return; /* (port 0, a SYN with FIN or RST: nobody sends those but to find out how we answer) */
  uint32_t seq = be32(seg + 4), ack = be32(seg + 8);
  uint16_t win = be16(seg + 14);
  const uint8_t *data = seg + hl;
  size_t dlen = len - hl;
  struct tcb *t = lookup(dst, dport, src, sport);
  if (!t) {
    if (flags & RST)
      return;
    struct sock *l = NULL;
    if (flags & (SYN | ACK)) {
      struct inaddr d, s;
      in_set4(&d, dst, dport);
      in_set4(&s, src, sport);
      l = in_find(6, &d, &s);
      if (l && l->state != SS_LISTENING) {
        sock_put(l);
        l = NULL;
      }
    }
    if (l && (flags & (SYN | ACK)) == SYN) {
      passive_open(l, src, dst, seg, hl);
      sock_put(l);
      return;
    }
    if (l && !(flags & SYN)) { /* an ACK to a listener: the end of a handshake that was answered with a cookie? */
      bool full = sock_backlog_full(l);
      if (!full)
        t = cookie_open(l, src, dst, seg);
      sock_put(l);
      if (full)
        return; /* (nothing to put it on: the peer sends again) */
    } else if (l) {
      sock_put(l);
    }
    if (!t) {
      reset_for(src, dst, seg, dlen);
      return;
    }
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
    t->snd_wl1 = seq;
    t->snd_wl2 = ack;
    if (ackok && !sbuf_get(t)) {
      tcb_abort(t, ENOBUFS);
      return;
    }
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

  if (t->state == SYN_RCVD && (flags & (SYN | ACK | RST)) == SYN && seq == t->irs) {
    xmit(t, t->iss, SYN | ACK, NULL, 0); /* the SYN again: our SYN-ACK did not get there */
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
    if (seq != t->rcv_nxt) { /* in the window, but not where the peer would reset: it may not be the peer */
      challenge_ack(t);
      return;
    }
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
  if (flags & SYN) { /* (a SYN in a connection that is up: the peer is asked, the connection stays) */
    challenge_ack(t);
    return;
  }
  if (!(flags & ACK))
    return;

  if (t->state == SYN_RCVD) {
    if (!(GT(ack, t->snd_una) && LEQ(ack, t->snd_nxt))) {
      reset_for(src, dst, seg, dlen);
      return;
    }
    struct sock *l = t->listener, *c = t->s;
    if (l) {
      if (!c || l->state != SS_LISTENING || !sbuf_get(t)) {
        xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
        tcb_free(t);
        if (c)
          sock_put(c);
        return;
      }
      if (sock_backlog_full(l))
        return; /* (the accept queue is full: this ACK is ignored, the peer's next one may find room) */
    } else if (!sbuf_get(t)) { /* (a simultaneous open) */
      xmit(t, t->snd_nxt, RST | ACK, NULL, 0);
      tcb_abort(t, ENOBUFS);
      return;
    }
    t->snd_una = ack;
    t->snd_wnd = win;
    t->snd_wl1 = seq;
    t->snd_wl2 = ack;
    t->state = ESTABLISHED;
    established(t);
    if (l) {
      t->listener = NULL;
      l->nsyn--;
      sock_add_backlog(l, c);
      sock_put(l);
    }
  }

  if (GT(ack, t->snd_max) || LT(ack, t->snd_una - MAXWIN)) { /* acknowledges what was never sent, or is far too old */
    challenge_ack(t);
    return;
  }
  uint32_t una0 = t->snd_una;
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
  if (GEQ(ack, una0) && (LT(t->snd_wl1, seq) || (t->snd_wl1 == seq && LEQ(t->snd_wl2, ack)))) {
    t->snd_wnd = win; /* (an older segment, one that came late, does not set the window) */
    t->snd_wl1 = seq;
    t->snd_wl2 = ack;
  }
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
    } else if (GT(seq, t->rcv_nxt) && t->s) {
      uint32_t off = seq - t->rcv_nxt;
      if (off < w)
        ooo_add(t, seq, data, MIN(dlen, (size_t)(w - off))); /* (what is inside the window) */
    }
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
  uint32_t seq = be32(hdr + 4); /* RFC 5927: only an error about a segment that is in flight is believed */
  if (!(GEQ(seq, t->snd_una) && LT(seq, t->snd_max)))
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
      int max = t->state == SYN_SENT ? 6 : t->state == SYN_RCVD ? 5 : t->s ? 12 : 8; /* (a closed socket's: less) */
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
        if (t->s) /* (nobody is waiting for the data of a closed socket: it is not asked for for ever) */
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
