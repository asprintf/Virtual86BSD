/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * Copyright (c) 2026, asprintf
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

/* if_var.h - network interfaces (net/if.c), the routing table
   (net/route.c), and IP as the drivers and the rest see it
   (netinet/ip.c).  Names and flag values are the BSDs' (struct ifnet,
   IFF_*, RTF_*); a driver attaches with if_attach_ether(). */
#ifndef _NET_IF_VAR_H_
#define _NET_IF_VAR_H_

#include <sys/systm.h>
#include <net/if.h>
#include <net/route.h>

#define IF_MAXIF 8   /* interfaces, lo0 included */
#define IF_MAXADDR 4 /* IPv4 addresses on one interface */

/* an IPv4 address of an interface (host order); its broadcast address is
   addr | ~mask */
struct ifaddr4 {
  bool used;
  uint32_t addr, mask;
};

struct ifnet {
  char if_xname[IFNAMSIZ]; /* "em0", "vio0", "lo0" */
  int if_index;            /* 1 is lo0 */
  unsigned if_flags;       /* IFF_ */
  unsigned if_mtu, if_hardmtu; /* the card's own largest */
  int if_metric;           /* of its connected routes */
  bool if_ether;           /* an Ethernet: if_lladdr, ARP, if_xmit */
  uint8_t if_lladdr[6];
  struct ifaddr4 if_addrs[IF_MAXADDR];
  uint64_t if_ipackets, if_ibytes, if_idrops, if_opackets, if_obytes, if_odrops;
  /* the driver: a whole frame out (0 or -errno); the received frames to
     net_rx() (the network thread calls it); the device's interrupt on
     (the interface goes up) */
  int (*if_xmit)(struct ifnet *, const void *frame, size_t len);
  void (*if_poll)(struct ifnet *);
  void (*if_start)(struct ifnet *);
  void *if_softc;
};

extern struct ifnet *ifnet[IF_MAXIF]; /* by index - 1 */
extern int nifnet;

struct ifnet *if_attach_ether(const char *driver, const uint8_t *mac, unsigned mtu,
                              int (*xmit)(struct ifnet *, const void *, size_t), void (*poll)(struct ifnet *),
                              void (*start)(struct ifnet *), void *softc);
struct ifnet *if_byname(const char *name);
struct ifnet *if_byindex(int index);
void if_up(struct ifnet *ifp);
void if_down(struct ifnet *ifp);
int ifa_add(struct ifnet *ifp, uint32_t addr, uint32_t mask); /* also its connected route */
bool sa_get4(const void *sa, uint32_t *ip); /* a sockaddr_in (AF_INET): its address, host order */
void sa_put4(void *sa, uint32_t ip);
long ifioctl(unsigned cmd, uint64_t arg); /* the socket ioctls of <sys/sockio.h> (net/if.c, net/route.c) */
long rtioctl(unsigned cmd, uint64_t arg);
int rt_dump(void *buf, int max);          /* struct rtinfo of the table (<net/route.h>), the bytes */
int ifa_remove(struct ifnet *ifp, uint32_t addr);
uint32_t if_addr(const struct ifnet *ifp); /* its first address, or 0 */
bool if_has_addr(const struct ifnet *ifp, uint32_t addr);
bool if_is_bcast(const struct ifnet *ifp, uint32_t addr); /* a subnet broadcast of it */

/* the routing table */
#define RTMAX 32
struct rtentry {
  bool used;
  uint32_t dst, mask, gw; /* host order; gw 0: on the link */
  struct ifnet *ifp;
  unsigned flags;         /* RTF_ */
  int metric;
};
extern struct rtentry rtable[RTMAX];
int rt_add(uint32_t dst, uint32_t mask, uint32_t gw, struct ifnet *ifp, int metric);
int rt_delete(uint32_t dst, uint32_t mask);
struct rtentry *rt_lookup(uint32_t dst); /* longest prefix, then the lowest metric; the interface is up */
void rt_prune(void);                     /* drop the routes whose gateway is no longer on a connected network */

/* what DHCP (or ip=) told about the network as a whole */
struct netinfo {
  bool dhcp;
  uint32_t dns[3]; /* host order */
  int ndns;
  char domain[64];
};
extern struct netinfo netinfo;

void net_init_conf(const char *ipconf, const char *loss); /* ip=, netloss= */
void dhcp_stop(struct ifnet *ifp); /* an address was set by hand: the kernel's DHCP client leaves ifp alone */
void net_pnp(void (*out)(void *, const char *, ...), void *sb);
void net_rx(struct ifnet *ifp, const void *frame, size_t len); /* a driver: a frame came (from the network thread) */
void net_wake(void);                         /* (any context) */
bool ip_is_local(uint32_t ip);               /* one of this system's addresses (or 127.x) */
bool ip_routable(uint32_t ip);               /* reachable through an interface */
uint32_t ip_srcaddr(uint32_t dst);           /* the address a packet to dst leaves from, or 0 */
int ip_output(int proto, uint32_t src, uint32_t dst, const void *payload, size_t len);
int ip_output_raw(void *pkt, size_t len);    /* a whole IPv4 packet */
uint16_t ip_csum(const void *p, size_t n, uint32_t sum);
uint32_t ip_pseudo(uint32_t src, uint32_t dst, int proto, size_t len);
uint64_t net_now_ms(void);

#endif
