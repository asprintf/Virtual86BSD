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

/* ip.c - IPv4 on the network interfaces (net/if.c): Ethernet framing, ARP
   per interface, IP input and output (routes from net/route.c;
   fragmenting on output, reassembling on input: up to 8 datagrams at
   once, 30 seconds each), ICMP (echo, unreachable), UDP to and from the
   sockets, and a DHCP client.  The first Ethernet interface is configured
   by DHCP or by ip= on the kernel command line (ip=dhcp, ip=off,
   ip=ADDR/PREFIX[,GATEWAY[,DNS]]), the others by ifconfig(8);
   /proc/net/pnp tells the name server, as on Linux.  For testing,
   netloss=N on the command line loses every Nth frame each way.

   All of it runs in the network thread (frames received, timers) or in a
   system call (sending); the card's interrupt only wakes the thread.  The
   kernel is not preemptive, so nothing here is entered twice at once. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <sys/socketvar.h>
#include <machine/cpu.h>

static struct waitq netwq;
static volatile bool net_pending;

#define ETH_IP 0x0800
#define ETH_ARP 0x0806
#define MTU 1500

static inline uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static inline void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v; }
static inline void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

uint64_t net_now_ms(void) { return monotonic_ns() / 1000000UL; }

uint16_t ip_csum(const void *buf, size_t n, uint32_t sum) {
  const uint8_t *p = buf;
  for (; n > 1; p += 2, n -= 2)
    sum += (uint32_t)(p[0] << 8 | p[1]);
  if (n)
    sum += (uint32_t)(p[0] << 8);
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);
  return (uint16_t)~sum;
}

/* the pseudo-header's sum, to start ip_csum of a TCP or UDP segment with */
uint32_t ip_pseudo(uint32_t src, uint32_t dst, int proto, size_t len) {
  return (src >> 16) + (src & 0xffff) + (dst >> 16) + (dst & 0xffff) + (uint32_t)proto + (uint32_t)len;
}

bool ip_is_local(uint32_t ip) {
  if ((ip >> 24) == 127)
    return true;
  for (int i = 0; i < nifnet; i++)
    if (if_has_addr(ifnet[i], ip))
      return true;
  return false;
}

/* an Ethernet interface that is up to send a broadcast from (with src, the
   one that has that address) */
static struct ifnet *eth_for(uint32_t src) {
  struct ifnet *first = NULL;
  for (int i = 0; i < nifnet; i++) {
    struct ifnet *ifp = ifnet[i];
    if (!ifp->if_ether || !(ifp->if_flags & IFF_UP))
      continue;
    if (src && if_has_addr(ifp, src))
      return ifp;
    if (!first)
      first = ifp;
  }
  return first;
}

/* the address of ifp a packet leaves from towards hop: one on that network,
   else the first */
static uint32_t srcfor(const struct ifnet *ifp, uint32_t hop) {
  for (int i = 0; i < IF_MAXADDR; i++) {
    const struct ifaddr4 *a = &ifp->if_addrs[i];
    if (a->used && (hop & a->mask) == (a->addr & a->mask))
      return a->addr;
  }
  return if_addr(ifp);
}

bool ip_routable(uint32_t ip) {
  if (ip == 0)
    return false;
  if (ip == 0xffffffffU)
    return eth_for(0) != NULL;
  struct rtentry *r = rt_lookup(ip);
  return r && r->ifp->if_ether;
}

uint32_t ip_srcaddr(uint32_t dst) {
  struct rtentry *r = rt_lookup(dst);
  return r ? srcfor(r->ifp, r->gw ? r->gw : dst) : 0;
}

/* ---- Ethernet and ARP ---- */

static unsigned loss_every, loss_tx, loss_rx;

static void eth_send(struct ifnet *ifp, const uint8_t *dmac, uint16_t type, const void *payload, size_t len) {
  if (loss_every && ++loss_tx % loss_every == 0)
    return;
  uint8_t f[MTU + 14];
  memcpy(f, dmac, 6);
  memcpy(f + 6, ifp->if_lladdr, 6);
  put16(f + 12, type);
  memcpy(f + 14, payload, len);
  if (ifp->if_xmit(ifp, f, len + 14) == 0) {
    ifp->if_opackets++;
    ifp->if_obytes += len + 14;
  }
}

#define NARP 32
#define ARP_PENDING 8
struct pend {
  struct pend *next;
  size_t len;
  uint8_t data[];
};
static struct arp {
  struct ifnet *ifp;
  uint32_t ip;
  uint8_t mac[6];
  int state; /* 0 free, 1 asking, 2 known */
  uint64_t expires, next_try;
  int tries, npend;
  struct pend *pend;
} arps[NARP];

static const uint8_t bmac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static void arp_request(struct ifnet *ifp, uint32_t ip) {
  uint8_t a[28];
  put16(a, 1), put16(a + 2, ETH_IP), a[4] = 6, a[5] = 4, put16(a + 6, 1);
  memcpy(a + 8, ifp->if_lladdr, 6);
  put32(a + 14, srcfor(ifp, ip));
  memset(a + 18, 0, 6);
  put32(a + 24, ip);
  eth_send(ifp, bmac, ETH_ARP, a, 28);
}

static struct arp *arp_lookup(struct ifnet *ifp, uint32_t ip) {
  for (int i = 0; i < NARP; i++)
    if (arps[i].state && arps[i].ifp == ifp && arps[i].ip == ip)
      return &arps[i];
  return NULL;
}

static void arp_drop(struct arp *e) {
  while (e->pend) {
    struct pend *p = e->pend;
    e->pend = p->next;
    kfree(p);
  }
  e->npend = 0;
  e->state = 0;
}

static struct arp *arp_slot(void) {
  struct arp *old = &arps[0];
  for (int i = 0; i < NARP; i++) {
    if (!arps[i].state)
      return &arps[i];
    if (arps[i].expires < old->expires)
      old = &arps[i];
  }
  arp_drop(old);
  return old;
}

static void arp_learn(struct ifnet *ifp, uint32_t ip, const uint8_t *mac, bool create) {
  struct arp *e = arp_lookup(ifp, ip);
  if (!e) {
    if (!create)
      return;
    e = arp_slot();
    e->ifp = ifp;
    e->ip = ip;
  }
  memcpy(e->mac, mac, 6);
  e->state = 2;
  e->expires = net_now_ms() + 600000;
  while (e->pend) { /* what waited for the address */
    struct pend *p = e->pend;
    e->pend = p->next;
    eth_send(e->ifp, e->mac, ETH_IP, p->data, p->len);
    kfree(p);
  }
  e->npend = 0;
}

static void arp_input(struct ifnet *ifp, const uint8_t *a, size_t len) {
  if (len < 28 || be16(a) != 1 || be16(a + 2) != ETH_IP || a[4] != 6 || a[5] != 4)
    return;
  uint32_t spa = be32(a + 14), tpa = be32(a + 24);
  bool forme = if_has_addr(ifp, tpa);
  if (spa)
    arp_learn(ifp, spa, a + 8, forme);
  if (forme && be16(a + 6) == 1) {
    uint8_t r[28];
    memcpy(r, a, 8);
    put16(r + 6, 2);
    memcpy(r + 8, ifp->if_lladdr, 6);
    put32(r + 14, tpa);
    memcpy(r + 18, a + 8, 6);
    put32(r + 24, spa);
    eth_send(ifp, a + 8, ETH_ARP, r, 28);
  }
}

/* send an IP packet to the next hop */
static int ip_xmit(struct ifnet *ifp, uint32_t hop, const void *pkt, size_t len) {
  if (hop == 0xffffffffU || if_is_bcast(ifp, hop)) {
    eth_send(ifp, bmac, ETH_IP, pkt, len);
    return 0;
  }
  struct arp *e = arp_lookup(ifp, hop);
  if (e && e->state == 2) {
    eth_send(ifp, e->mac, ETH_IP, pkt, len);
    return 0;
  }
  if (!e) {
    e = arp_slot();
    e->ifp = ifp;
    e->ip = hop;
    e->state = 1;
    e->tries = 1;
    e->expires = net_now_ms() + 10000;
    e->next_try = net_now_ms() + 1000;
    arp_request(ifp, hop);
    net_wake(); /* (the thread retries) */
  }
  if (e->npend >= ARP_PENDING)
    return 0; /* dropped, as a full queue would */
  struct pend *p = kmalloc(sizeof(*p) + len);
  if (!p)
    return -ENOBUFS;
  p->len = len;
  p->next = NULL;
  memcpy(p->data, pkt, len);
  struct pend **pp = &e->pend;
  while (*pp)
    pp = &(*pp)->next;
  *pp = p;
  e->npend++;
  return 0;
}

static uint64_t arp_timers(uint64_t now) {
  uint64_t next = 0;
  for (int i = 0; i < NARP; i++) {
    struct arp *e = &arps[i];
    if (e->state == 1) {
      if (now >= e->next_try) {
        if (e->tries >= 3) {
          arp_drop(e); /* no answer: what waited is lost (TCP retries) */
          continue;
        }
        e->tries++;
        e->next_try = now + 1000;
        arp_request(e->ifp, e->ip);
      }
      if (!next || e->next_try < next)
        next = e->next_try;
    } else if (e->state == 2 && now >= e->expires)
      e->state = 0;
  }
  return next;
}

/* ---- IP ---- */

/* the interface and the next hop for a packet to dst (from src: 0 for
   any); *err on failure */
static struct ifnet *route_out(uint32_t dst, uint32_t src, uint32_t *hop, int *err) {
  if (dst == 0xffffffffU) {
    struct ifnet *ifp = eth_for(src);
    if (!ifp)
      *err = -ENETUNREACH;
    *hop = dst;
    return ifp;
  }
  struct rtentry *r = rt_lookup(dst);
  if (!r || !r->ifp->if_ether) {
    *err = -ENETUNREACH;
    return NULL;
  }
  *hop = r->gw ? r->gw : dst;
  return r->ifp;
}

static uint16_t ip_id;

/* a packet out of ifp towards hop, in fragments if it does not fit */
static int ip_output_on(struct ifnet *ifp, uint32_t hop, int proto, uint32_t src, uint32_t dst, const void *payload,
                        size_t len) {
  size_t mtu = MIN(ifp->if_mtu, MTU);
  if (len > 65515)
    return -EMSGSIZE;
  uint16_t id = ++ip_id;
  const uint8_t *data = payload;
  size_t off = 0;
  do { /* fragments of at most MTU, offsets in 8-byte units */
    size_t n = len - off;
    bool more = false;
    if (n > mtu - 20) {
      n = (mtu - 20) & ~7UL;
      more = true;
    }
    uint8_t pkt[MTU];
    memset(pkt, 0, 20);
    pkt[0] = 0x45;
    put16(pkt + 2, (uint16_t)(n + 20));
    put16(pkt + 4, id);
    put16(pkt + 6, (uint16_t)((more ? 0x2000 : 0) | (off >> 3) | (off == 0 && !more && proto == 6 ? 0x4000 : 0)));
    pkt[8] = 64;
    pkt[9] = (uint8_t)proto;
    put32(pkt + 12, src);
    put32(pkt + 16, dst);
    put16(pkt + 10, ip_csum(pkt, 20, 0));
    memcpy(pkt + 20, data + off, n);
    int r = ip_xmit(ifp, hop, pkt, n + 20);
    if (r)
      return r;
    off += n;
  } while (off < len);
  return 0;
}

int ip_output(int proto, uint32_t src, uint32_t dst, const void *payload, size_t len) {
  int err = 0;
  uint32_t hop = 0;
  struct ifnet *ifp = route_out(dst, src, &hop, &err);
  if (err)
    return err;
  if (!src)
    src = srcfor(ifp, hop);
  return ip_output_on(ifp, hop, proto, src, dst, payload, len);
}

int ip_output_raw(void *buf, size_t len) {
  uint8_t *pkt = buf;
  int err = 0;
  if (len < 20 || len > MTU || (pkt[0] >> 4) != 4)
    return len > MTU ? -EMSGSIZE : -EINVAL;
  size_t hl = (size_t)(pkt[0] & 15) * 4;
  if (hl < 20 || hl > len)
    return -EINVAL;
  uint32_t dst = be32(pkt + 16), hop = 0;
  struct ifnet *ifp = route_out(dst, be32(pkt + 12), &hop, &err);
  if (err)
    return err;
  if (!be32(pkt + 12))
    put32(pkt + 12, srcfor(ifp, hop));
  if (!be16(pkt + 4))
    put16(pkt + 4, ++ip_id);
  put16(pkt + 2, (uint16_t)len);
  put16(pkt + 10, 0);
  put16(pkt + 10, ip_csum(pkt, hl, 0));
  return ip_xmit(ifp, hop, pkt, len);
}

/* ICMP destination unreachable about packet ip (header first) */
static void icmp_unreach(int code, const uint8_t *ip, size_t len) {
  size_t hl = (size_t)(ip[0] & 15) * 4;
  size_t n = MIN(len, hl + 8);
  uint8_t m[8 + 60 + 8];
  memset(m, 0, 8);
  m[0] = 3, m[1] = (uint8_t)code;
  memcpy(m + 8, ip, n);
  put16(m + 2, ip_csum(m, 8 + n, 0));
  ip_output(1, 0, be32(ip + 12), m, 8 + n);
}

static void icmp_input(uint32_t src, uint32_t dst, const uint8_t *m, size_t len) {
  if (len < 8 || ip_csum(m, len, 0) != 0)
    return;
  if (m[0] == 8 && m[1] == 0) { /* echo request */
    uint8_t *r = kmalloc(len);
    if (!r)
      return;
    memcpy(r, m, len);
    r[0] = 0;
    put16(r + 2, 0);
    put16(r + 2, ip_csum(r, len, 0));
    ip_output(1, dst, src, r, len);
    kfree(r);
  } else if (m[0] == 3 && len >= 8 + 20 + 8) { /* unreachable: about our packet */
    const uint8_t *ip = m + 8;
    size_t hl = (size_t)(ip[0] & 15) * 4;
    if (len < 8 + hl + 8)
      return;
    int err = m[1] == 3 ? ECONNREFUSED : m[1] == 0 ? ENETUNREACH : EHOSTUNREACH;
    struct inaddr ls, rd;
    in_set4(&ls, be32(ip + 12), be16(ip + hl));
    in_set4(&rd, be32(ip + 16), be16(ip + hl + 2));
    if (ip[9] == 17)
      udp_icmp_error(&ls, &rd, err);
    else if (ip[9] == 6)
      tcp_icmp_error(be32(ip + 12), be32(ip + 16), ip + hl, len - 8 - hl, err);
  }
}

static void dhcp_input(const uint8_t *p, size_t len);
static struct ifnet *dhcp_ifp; /* the interface the kernel's DHCP client runs on */
static uint16_t dhcp_xid_port = 68;
static bool dhcp_running;

static void udp_input(struct ifnet *ifp, uint32_t src, uint32_t dst, const uint8_t *u, size_t len, const uint8_t *ip,
                      size_t iplen) {
  if (len < 8 || be16(u + 4) < 8 || be16(u + 4) > len)
    return;
  len = be16(u + 4);
  if (be16(u + 6) && ip_csum(u, len, ip_pseudo(src, dst, 17, len)) != 0)
    return;
  uint16_t sp = be16(u), dp = be16(u + 2);
  if (dp == dhcp_xid_port && dhcp_running && ifp == dhcp_ifp) {
    dhcp_input(u + 8, len - 8);
    return;
  }
  struct inaddr d, s;
  bool broadcast = dst == 0xffffffffU || if_is_bcast(ifp, dst);
  in_set4(&d, broadcast ? if_addr(ifp) : dst, dp);
  in_set4(&s, src, sp);
  struct sock *t = in_find(17, &d, &s);
  if (!t) {
    if (!broadcast)
      icmp_unreach(3, ip, iplen); /* port unreachable */
    return;
  }
  udp_deliver(t, &s, u + 8, len - 8);
  sock_put(t);
}

/* ---- reassembly ---- */

#define NFRAG 8
static struct frag {
  bool used;
  uint32_t src, dst;
  uint16_t id;
  uint8_t proto;
  uint8_t *buf;     /* the datagram, header (the first fragment's) included */
  uint8_t *have;    /* a byte per 8-byte unit of the payload */
  size_t hl, total; /* total: payload length, once the last fragment came */
  uint64_t expires;
} frags[NFRAG];

static void frag_free(struct frag *f) {
  kfree(f->buf);
  kfree(f->have);
  f->used = false;
}

static void ip_input(struct ifnet *ifp, const uint8_t *p, size_t len);

static void frag_input(struct ifnet *ifp, const uint8_t *p, size_t hl, size_t tl) {
  uint32_t src = be32(p + 12), dst = be32(p + 16);
  uint16_t id = be16(p + 4), fo = be16(p + 6);
  size_t off = (size_t)(fo & 0x1fff) * 8, n = tl - hl;
  bool more = fo & 0x2000;
  if (off + n > 65515 || (more && (n & 7)))
    return;
  struct frag *f = NULL, *freeslot = NULL;
  uint64_t now = net_now_ms();
  for (int i = 0; i < NFRAG; i++) {
    if (frags[i].used && now >= frags[i].expires)
      frag_free(&frags[i]);
    if (frags[i].used && frags[i].src == src && frags[i].dst == dst && frags[i].id == id && frags[i].proto == p[9])
      f = &frags[i];
    else if (!frags[i].used && !freeslot)
      freeslot = &frags[i];
  }
  if (!f) {
    if (!freeslot)
      return;
    f = freeslot;
    f->buf = kmalloc(65535);
    f->have = kzalloc(65536 / 8);
    if (!f->buf || !f->have) {
      kfree(f->buf), kfree(f->have);
      return;
    }
    f->used = true;
    f->src = src, f->dst = dst, f->id = id, f->proto = p[9];
    f->hl = 20;
    f->total = 0;
    f->expires = now + 30000;
  }
  if (off == 0) { /* the first fragment's header */
    memcpy(f->buf, p, hl);
    f->hl = hl;
  }
  memcpy(f->buf + 60 + off, p + hl, n); /* (payload at 60: room for any header) */
  for (size_t u = off / 8; u < (off + n + 7) / 8; u++)
    f->have[u] = 1;
  if (!more)
    f->total = off + n;
  if (!f->total || !f->have[0])
    return;
  for (size_t u = 0; u < (f->total + 7) / 8; u++)
    if (!f->have[u])
      return;
  /* complete: header and payload together, as one unfragmented datagram */
  size_t whole = f->hl + f->total;
  memmove(f->buf + f->hl, f->buf + 60, f->total);
  put16(f->buf + 2, (uint16_t)whole);
  put16(f->buf + 6, 0);
  put16(f->buf + 10, 0);
  put16(f->buf + 10, ip_csum(f->buf, f->hl, 0));
  uint8_t *d = f->buf;
  f->buf = NULL;
  frag_free(f);
  ip_input(ifp, d, whole);
  kfree(d);
}

static void ip_input(struct ifnet *ifp, const uint8_t *p, size_t len) {
  if (len < 20 || (p[0] >> 4) != 4)
    return;
  size_t hl = (size_t)(p[0] & 15) * 4, tl = be16(p + 2);
  if (hl < 20 || tl < hl || tl > len || ip_csum(p, hl, 0) != 0)
    return;
  uint32_t src = be32(p + 12), dst = be32(p + 16);
  if ((src >> 24) == 127 || (dst >> 24) == 127) /* a loopback address does not come from a wire */
    return;
  bool unconfigured = !if_addr(ifp); /* (DHCP answers come before there is an address) */
  if (be16(p + 6) & 0x3fff) { /* a fragment */
    if (unconfigured || ip_is_local(dst))
      frag_input(ifp, p, hl, tl);
    return;
  }
  bool forme = unconfigured || ip_is_local(dst) || dst == 0xffffffffU || if_is_bcast(ifp, dst);
  if (!forme)
    return;
  int proto = p[9];
  struct inaddr s, d;
  in_set4(&s, src, 0);
  in_set4(&d, dst, 0);
  raw_deliver(proto, &s, &d, p, tl);
  const uint8_t *pl = p + hl;
  size_t n = tl - hl;
  switch (proto) {
  case 1: icmp_input(src, dst, pl, n); break;
  case 17: udp_input(ifp, src, dst, pl, n, p, tl); break;
  case 6:
    if (ip_is_local(dst))
      tcp_input(src, dst, pl, n);
    break;
  }
}

void net_rx(struct ifnet *ifp, const void *frame, size_t len) {
  const uint8_t *f = frame;
  ifp->if_ipackets++;
  ifp->if_ibytes += len;
  if (!(ifp->if_flags & IFF_UP)) {
    ifp->if_idrops++;
    return;
  }
  if (loss_every && ++loss_rx % loss_every == 0)
    return;
  if (memcmp(f, ifp->if_lladdr, 6) && memcmp(f, bmac, 6))
    return;
  uint16_t type = be16(f + 12);
  if (type == ETH_ARP)
    arp_input(ifp, f + 14, len - 14);
  else if (type == ETH_IP)
    ip_input(ifp, f + 14, len - 14);
}

/* ---- DHCP ---- */

static int dhcp_state; /* 0 idle, 1 discovering, 2 requesting, 3 bound */
static uint32_t dhcp_xid, dhcp_offer, dhcp_server;
static uint64_t dhcp_next;
static int dhcp_tries;

static void dhcp_send(int type) {
  uint8_t p[300 + 8];
  uint8_t *b = p + 8;
  memset(p, 0, sizeof(p));
  b[0] = 1, b[1] = 1, b[2] = 6;
  put32(b + 4, dhcp_xid);
  put16(b + 10, 0x8000); /* answer by broadcast */
  memcpy(b + 28, dhcp_ifp->if_lladdr, 6);
  put32(b + 236, 0x63825363);
  uint8_t *o = b + 240;
  *o++ = 53, *o++ = 1, *o++ = (uint8_t)type;
  *o++ = 55, *o++ = 4, *o++ = 1, *o++ = 3, *o++ = 6, *o++ = 15;
  if (type == 3) {
    *o++ = 50, *o++ = 4, put32(o, dhcp_offer), o += 4;
    *o++ = 54, *o++ = 4, put32(o, dhcp_server), o += 4;
  }
  *o++ = 255;
  size_t n = 300;
  put16(p, 68), put16(p + 2, 67), put16(p + 4, (uint16_t)(n + 8));
  put16(p + 6, 0);
  uint16_t c = ip_csum(p, n + 8, ip_pseudo(0, 0xffffffffU, 17, n + 8));
  put16(p + 6, c ? c : 0xffff);
  ip_output_on(dhcp_ifp, 0xffffffffU, 17, 0, 0xffffffffU, p, n + 8);
}

static int prefix_of(uint32_t mask) {
  int n = 0;
  while (mask & 0x80000000U)
    n++, mask <<= 1;
  return n;
}

static void dhcp_input(const uint8_t *b, size_t len) {
  if (len < 240 || b[0] != 2 || be32(b + 4) != dhcp_xid || be32(b + 236) != 0x63825363)
    return;
  int type = 0;
  uint32_t mask = 0, router = 0, server = 0, dns[3];
  int ndns = 0;
  char domain[64] = "";
  for (size_t i = 240; i < len && b[i] != 255;) {
    uint8_t opt = b[i];
    if (opt == 0) {
      i++;
      continue;
    }
    if (i + 2 > len)
      break;
    size_t l = b[i + 1];
    const uint8_t *v = b + i + 2;
    if (i + 2 + l > len)
      break;
    if (opt == 53 && l >= 1)
      type = v[0];
    else if (opt == 1 && l >= 4)
      mask = be32(v);
    else if (opt == 3 && l >= 4)
      router = be32(v);
    else if (opt == 54 && l >= 4)
      server = be32(v);
    else if (opt == 6)
      for (size_t k = 0; k + 4 <= l && ndns < 3; k += 4)
        dns[ndns++] = be32(v + k);
    else if (opt == 15 && l < sizeof(domain)) {
      memcpy(domain, v, l);
      domain[l] = 0;
    }
    i += 2 + l;
  }
  uint32_t yi = be32(b + 16);
  if (type == 2 && dhcp_state == 1) { /* offer */
    dhcp_offer = yi;
    dhcp_server = server;
    dhcp_state = 2;
    dhcp_tries = 0;
    dhcp_send(3);
    dhcp_next = net_now_ms() + 2000;
  } else if (type == 5 && dhcp_state == 2) { /* ack */
    uint32_t m = mask ? mask : 0xffffff00U;
    ifa_add(dhcp_ifp, yi, m);
    if (router)
      rt_add(0, 0, router, dhcp_ifp, 0);
    netinfo.ndns = ndns;
    for (int k = 0; k < ndns; k++)
      netinfo.dns[k] = dns[k];
    strcpy(netinfo.domain, domain);
    dhcp_state = 3;
    dhcp_running = false;
    printf("%s: %u.%u.%u.%u/%d by DHCP, gateway %u.%u.%u.%u\n", dhcp_ifp->if_xname, yi >> 24, (yi >> 16) & 255,
           (yi >> 8) & 255, yi & 255, prefix_of(m), router >> 24, (router >> 16) & 255, (router >> 8) & 255,
           router & 255);
  } else if (type == 6 && dhcp_state == 2) { /* nak: again */
    dhcp_state = 0;
  }
}

static uint64_t dhcp_timers(uint64_t now) {
  if (!dhcp_running)
    return 0;
  if (dhcp_state == 0 || (now >= dhcp_next && dhcp_tries >= 4)) {
    uint64_t r;
    get_random_bytes(&r, sizeof(r));
    dhcp_xid = (uint32_t)r;
    dhcp_state = 1;
    dhcp_tries = 0;
    dhcp_next = 0;
  }
  if (now >= dhcp_next) {
    dhcp_tries++;
    dhcp_send(dhcp_state == 1 ? 1 : 3);
    dhcp_next = now + (dhcp_tries < 4 ? 1000UL << dhcp_tries : 30000);
  }
  return dhcp_next;
}

/* ---- the thread ---- */

void net_wake(void) {
  net_pending = true;
  wake_up(&netwq);
}

static void net_work(void) {
  for (int i = 0; i < nifnet; i++)
    if (ifnet[i]->if_poll)
      ifnet[i]->if_poll(ifnet[i]);
}

static long net_thread(void *arg) {
  (void)arg;
  for (;;) {
    net_pending = false;
    net_work();
    uint64_t now = net_now_ms();
    uint64_t next = 0, t;
    if ((t = arp_timers(now)) && (!next || t < next))
      next = t;
    if ((t = dhcp_timers(now)) && (!next || t < next))
      next = t;
    if ((t = tcp_timers(now)) && (!next || t < next))
      next = t;
    uint64_t f = irq_save();
    if (!net_pending)
      sleep_on(&netwq, next ? next * 1000000UL : 0, false);
    irq_restore(f);
  }
  return 0;
}

static bool parse_ip(const char **sp, uint32_t *out) {
  const char *s = *sp;
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) {
    if (*s < '0' || *s > '9')
      return false;
    uint32_t b = 0;
    while (*s >= '0' && *s <= '9')
      b = b * 10 + (uint32_t)(*s++ - '0');
    if (b > 255)
      return false;
    v = v << 8 | b;
    if (i < 3 && *s++ != '.')
      return false;
  }
  *out = v;
  *sp = s;
  return true;
}

static struct ifnet *first_ether(void) {
  for (int i = 0; i < nifnet; i++)
    if (ifnet[i]->if_ether)
      return ifnet[i];
  return NULL;
}

void net_init_conf(const char *conf, const char *loss) {
  waitq_init(&netwq);
  for (; *loss >= '0' && *loss <= '9'; loss++)
    loss_every = loss_every * 10 + (unsigned)(*loss - '0');
  struct ifnet *ifp = first_ether();
  if (!ifp) /* no card: its driver did not find or set one up */
    return;
  const char *nm = ifp->if_xname;
  if_up(ifp);
  if (!strcmp(conf, "off")) {
    printf("%s: not configured (ip=off)\n", nm);
  } else if (conf[0] && strcmp(conf, "dhcp")) {
    const char *s = conf;
    uint32_t a, g = 0, d = 0;
    int prefix = 24;
    if (!parse_ip(&s, &a)) {
      printf("%s: bad ip=%s\n", nm, conf);
      return;
    }
    if (*s == '/') {
      s++;
      prefix = 0;
      while (*s >= '0' && *s <= '9')
        prefix = prefix * 10 + *s++ - '0';
      if (prefix < 1 || prefix > 32)
        prefix = 24;
    }
    if (*s == ',' && (s++, parse_ip(&s, &g)) && *s == ',')
      s++, parse_ip(&s, &d);
    ifa_add(ifp, a, prefix == 32 ? 0xffffffffU : ~((1U << (32 - prefix)) - 1));
    if (g)
      rt_add(0, 0, g, ifp, 0);
    if (d)
      netinfo.dns[0] = d, netinfo.ndns = 1;
    printf("%s: %s\n", nm, conf);
  } else {
    /* DHCP: wait for it here a few seconds (by polling: the thread is not
       there yet), then the thread goes on trying */
    netinfo.dhcp = true;
    dhcp_ifp = ifp;
    dhcp_running = true;
    uint64_t until = net_now_ms() + 4000;
    while (!if_addr(ifp) && net_now_ms() < until) {
      dhcp_timers(net_now_ms());
      if (ifp->if_poll)
        ifp->if_poll(ifp);
      arp_timers(net_now_ms());
    }
    if (!if_addr(ifp))
      printf("%s: no DHCP answer yet\n", nm);
  }
  if (loss_every)
    printf("%s: losing every %uth frame (netloss=)\n", nm, loss_every);
  kthread_create("net", net_thread, NULL);
}

/* /proc/net/pnp, as Linux writes it after ip=dhcp: resolv.conf syntax */
void net_pnp(void (*out)(void *, const char *, ...), void *sb) {
  if (netinfo.dhcp)
    out(sb, "#PROTO: DHCP\n");
  if (netinfo.domain[0])
    out(sb, "domain %s\n", netinfo.domain);
  for (int i = 0; i < netinfo.ndns; i++)
    out(sb, "nameserver %u.%u.%u.%u\n", netinfo.dns[i] >> 24, (netinfo.dns[i] >> 16) & 255,
        (netinfo.dns[i] >> 8) & 255, netinfo.dns[i] & 255);
}
