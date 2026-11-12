/*
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

/* if.c - the network interfaces: lo0, and what the drivers attach
   (em0, vio0 ...).  An interface has its flags, its IPv4 addresses (each
   with its connected route in the table, net/route.c) and the driver's
   functions; IP (netinet/ip.c) does the rest.  Loopback traffic stays
   inside the socket layer (kern/uipc_socket.c): lo0 is here for its
   addresses, flags and the interface ioctls. */
#include <sys/param.h>
#include <sys/systm.h>
#include <net/if_var.h>
#include <uvm/uvm.h>

struct ifnet *ifnet[IF_MAXIF];
int nifnet;
struct netinfo netinfo;

static struct ifnet lo0;

static void if_init(void) {
  if (nifnet)
    return;
  strcpy(lo0.if_xname, "lo0");
  lo0.if_index = 1;
  lo0.if_flags = IFF_UP | IFF_RUNNING | IFF_LOOPBACK | IFF_MULTICAST;
  lo0.if_mtu = 65536;
  ifnet[0] = &lo0;
  nifnet = 1;
  ifa_add(&lo0, 0x7f000001U, 0xff000000U);
}

struct ifnet *if_byindex(int index) {
  if_init();
  return index >= 1 && index <= nifnet ? ifnet[index - 1] : NULL;
}

struct ifnet *if_byname(const char *name) {
  if_init();
  for (int i = 0; i < nifnet; i++)
    if (!strcmp(ifnet[i]->if_xname, name))
      return ifnet[i];
  return NULL;
}

/* a driver's interface: named driver and the next free unit ("em" is em0,
   a second one em1); down until if_up() */
struct ifnet *if_attach_ether(const char *driver, const uint8_t *mac, unsigned mtu,
                              int (*xmit)(struct ifnet *, const void *, size_t), void (*poll)(struct ifnet *),
                              void (*start)(struct ifnet *), void *softc) {
  if_init();
  if (nifnet == IF_MAXIF)
    return NULL;
  struct ifnet *ifp = kzalloc(sizeof(*ifp));
  if (!ifp)
    return NULL;
  int unit = 0;
  do
    snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", driver, unit++);
  while (if_byname(ifp->if_xname));
  ifp->if_index = nifnet + 1;
  ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
  ifp->if_mtu = mtu;
  ifp->if_ether = true;
  memcpy(ifp->if_lladdr, mac, 6);
  ifp->if_xmit = xmit;
  ifp->if_poll = poll;
  ifp->if_start = start;
  ifp->if_softc = softc;
  ifnet[nifnet++] = ifp;
  return ifp;
}

void if_up(struct ifnet *ifp) {
  if (ifp->if_flags & IFF_UP)
    return;
  if (ifp->if_start)
    ifp->if_start(ifp);
  ifp->if_flags |= IFF_UP | IFF_RUNNING;
}

/* (the device keeps running; net_rx drops, and the routes through it are not used) */
void if_down(struct ifnet *ifp) { ifp->if_flags &= ~(unsigned)(IFF_UP | IFF_RUNNING); }

uint32_t if_addr(const struct ifnet *ifp) {
  for (int i = 0; i < IF_MAXADDR; i++)
    if (ifp->if_addrs[i].used)
      return ifp->if_addrs[i].addr;
  return 0;
}

bool if_has_addr(const struct ifnet *ifp, uint32_t addr) {
  for (int i = 0; i < IF_MAXADDR; i++)
    if (ifp->if_addrs[i].used && ifp->if_addrs[i].addr == addr)
      return true;
  return false;
}

bool if_is_bcast(const struct ifnet *ifp, uint32_t addr) {
  for (int i = 0; i < IF_MAXADDR; i++) {
    const struct ifaddr4 *a = &ifp->if_addrs[i];
    if (a->used && a->mask != 0xffffffffU && addr == (a->addr | ~a->mask))
      return true;
  }
  return false;
}

/* another address of the interface covers the same network */
static bool same_net_left(const struct ifnet *ifp, const struct ifaddr4 *skip, uint32_t net, uint32_t mask) {
  for (int i = 0; i < IF_MAXADDR; i++) {
    const struct ifaddr4 *a = &ifp->if_addrs[i];
    if (a != skip && a->used && a->mask == mask && (a->addr & a->mask) == net)
      return true;
  }
  return false;
}

/* add an address (or change the mask of one); its network is a connected
   route: on the link, a host route for /32 */
int ifa_add(struct ifnet *ifp, uint32_t addr, uint32_t mask) {
  if (~mask & (~mask + 1)) /* not a prefix */
    return -EINVAL;
  struct ifaddr4 *slot = NULL, *same = NULL;
  for (int k = 0; k < nifnet; k++) {
    struct ifnet *o = ifnet[k];
    for (int i = 0; i < IF_MAXADDR; i++) {
      struct ifaddr4 *a = &o->if_addrs[i];
      if (a->used && a->addr == addr) {
        if (o != ifp)
          return -EADDRINUSE;
        same = a;
      } else if (!a->used && o == ifp && !slot)
        slot = a;
    }
  }
  if (same) {
    if (same->mask == mask)
      return 0;
    uint32_t onet = same->addr & same->mask;
    if (!same_net_left(ifp, same, onet, same->mask))
      rt_delete(onet, same->mask);
    same->mask = mask;
  } else {
    if (!slot)
      return -ENOSPC;
    slot->used = true;
    slot->addr = addr;
    slot->mask = mask;
  }
  rt_add(addr & mask, mask, 0, ifp, 0); /* (EEXIST: an alias on the same network) */
  return 0;
}

int ifa_remove(struct ifnet *ifp, uint32_t addr) {
  for (int i = 0; i < IF_MAXADDR; i++) {
    struct ifaddr4 *a = &ifp->if_addrs[i];
    if (!a->used || a->addr != addr)
      continue;
    uint32_t net = a->addr & a->mask, mask = a->mask;
    a->used = false;
    if (!same_net_left(ifp, a, net, mask))
      rt_delete(net, mask);
    rt_prune();
    return 0;
  }
  return -EADDRNOTAVAIL;
}
