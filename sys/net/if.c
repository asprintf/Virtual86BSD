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
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/socket.h>
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
  lo0.if_mtu = lo0.if_hardmtu = 65536;
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
  ifp->if_mtu = ifp->if_hardmtu = mtu;
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
  rt_add(addr & mask, mask, 0, ifp, ifp->if_metric); /* (EEXIST: an alias on the same network) */
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
    for (int k = i; k < IF_MAXADDR - 1; k++) /* (the order of the addresses stays) */
      ifp->if_addrs[k] = ifp->if_addrs[k + 1];
    ifp->if_addrs[IF_MAXADDR - 1].used = false;
    rt_prune();
    return 0;
  }
  return -EADDRNOTAVAIL;
}

/* ---- the interface ioctls (<sys/sockio.h>) ----
   Linux's struct ifreq is the one of <net/if.h>: the name, 16 bytes, then
   a sockaddr_in or an int or a short, 40 bytes in all.  A program of the
   Linux emulation sees Linux's IFF_ values, a native one the BSDs'. */

bool sa_get4(const void *sa, uint32_t *ip) {
  const uint8_t *s = sa;
  if (s[0] != AF_INET || s[1])
    return false;
  *ip = (uint32_t)s[4] << 24 | (uint32_t)s[5] << 16 | (uint32_t)s[6] << 8 | s[7];
  return true;
}

void sa_put4(void *sa, uint32_t ip) {
  uint8_t *s = sa;
  memset(s, 0, 16);
  s[0] = AF_INET;
  s[4] = (uint8_t)(ip >> 24), s[5] = (uint8_t)(ip >> 16), s[6] = (uint8_t)(ip >> 8), s[7] = (uint8_t)ip;
}

/* IFF_ as Linux has them: the first ten are the same, the BSD-only ones go
   and IFF_MULTICAST moves */
static unsigned linux_flags(unsigned f) { return (f & 0x3ffU) | ((f & IFF_MULTICAST) ? 0x1000U : 0); }

static uint32_t class_mask(uint32_t a) { return a < 0x80000000U ? 0xff000000U : a < 0xc0000000U ? 0xffff0000U : 0xffffff00U; }

static struct ifaddr4 *first_addr(struct ifnet *ifp) {
  for (int k = 0; k < IF_MAXADDR; k++)
    if (ifp->if_addrs[k].used)
      return &ifp->if_addrs[k];
  return NULL;
}

/* an address an interface may have: not 0, not all ones, not a multicast
   or reserved one, and 127.x only on the loopback */
static bool addr_ok(const struct ifnet *ifp, uint32_t a) {
  if (a == 0 || a == 0xffffffffU || a >= 0xe0000000U)
    return false;
  return (a >> 24) != 127 || (ifp->if_flags & IFF_LOOPBACK);
}

static long if_setflags(struct ifnet *ifp, unsigned want, bool lx) {
  unsigned cur = ifp->if_flags;
  if (lx) /* (it cannot see the BSD-only bits: those stay) */
    want = (want & 0x3ffU) | (cur & ~0x3ffU);
  want = (want & ~(unsigned)IFF_CANTCHANGE) | (cur & IFF_CANTCHANGE);
  if ((want ^ cur) & (IFF_PROMISC | IFF_ALLMULTI | IFF_NOARP | IFF_STATICARP))
    return -EOPNOTSUPP; /* (nothing below the drivers does them) */
  unsigned keep = IFF_DEBUG | IFF_LINK0 | IFF_LINK1 | IFF_LINK2;
  ifp->if_flags = (ifp->if_flags & ~keep) | (want & keep);
  if (want & IFF_UP)
    if_up(ifp);
  else
    if_down(ifp);
  return 0;
}

/* the first address of an interface becomes ip, in its place (its mask
   stays; a first one gets that of its class) */
static long if_setaddr(struct ifnet *ifp, uint32_t ip) {
  struct ifaddr4 *a = first_addr(ifp);
  if (!addr_ok(ifp, ip))
    return -EINVAL;
  if (!a)
    return ifa_add(ifp, ip, class_mask(ip));
  if (a->addr == ip)
    return 0;
  for (int k = 0; k < nifnet; k++)
    for (int i = 0; i < IF_MAXADDR; i++)
      if (ifnet[k]->if_addrs[i].used && ifnet[k]->if_addrs[i].addr == ip)
        return -EADDRINUSE;
  uint32_t mask = a->mask, onet = a->addr & mask;
  bool keep = (ip & mask) == onet || same_net_left(ifp, a, onet, mask);
  a->addr = ip;
  if (!keep)
    rt_delete(onet, mask);
  rt_add(ip & mask, mask, 0, ifp, ifp->if_metric); /* (EEXIST: the network was there) */
  rt_prune();
  return 0;
}

static long if_setmetric(struct ifnet *ifp, int m) {
  if (m < 0 || m > 255)
    return -EINVAL;
  ifp->if_metric = m;
  for (int i = 0; i < RTMAX; i++)
    if (rtable[i].used && rtable[i].ifp == ifp && !rtable[i].gw)
      rtable[i].metric = m;
  return 0;
}

long ifioctl(unsigned cmd, uint64_t arg) {
  bool lx = current->proc->p_emul == &emul_linux;
  char r[64];
  int32_t n;
  if (cmd == SIOCGIFCONF) { /* struct ifconf { int len; char *buf; }: an entry per address */
    int32_t len;
    uint64_t ubuf;
    if (copy_from_user(&len, (void *)arg, 4) || copy_from_user(&ubuf, (char *)arg + 8, 8))
      return -EFAULT;
    int32_t out = 0;
    for (int i = 1; i <= nifnet; i++) {
      struct ifnet *ifp = if_byindex(i);
      for (int k = 0; k < IF_MAXADDR; k++) {
        if (!ifp->if_addrs[k].used)
          continue;
        if (ubuf && len >= out + 40) {
          memset(r, 0, 40);
          strcpy(r, ifp->if_xname);
          sa_put4(r + 16, ifp->if_addrs[k].addr);
          if (copy_to_user((char *)ubuf + out, r, 40))
            return -EFAULT;
        }
        out += 40;
      }
    }
    return copy_to_user((void *)arg, &out, 4) ? -EFAULT : 0;
  }
  bool set = cmd == SIOCSIFFLAGS || cmd == SIOCSIFADDR || cmd == SIOCSIFDSTADDR || cmd == SIOCSIFBRDADDR ||
             cmd == SIOCSIFNETMASK || cmd == SIOCSIFMETRIC || cmd == SIOCSIFMTU || cmd == SIOCAIFADDR ||
             cmd == SIOCDIFADDR;
  if (set && !capable())
    return -EPERM;
  size_t size = cmd == SIOCAIFADDR ? 64 : 40;
  if (copy_from_user(r, (void *)arg, size))
    return -EFAULT;
  memcpy(&n, r + 16, 4);
  if (cmd == SIOCGIFNAME) {
    struct ifnet *ifp = if_byindex(n);
    if (!ifp)
      return -ENODEV;
    memset(r, 0, 16);
    strcpy(r, ifp->if_xname);
    return copy_to_user((void *)arg, r, 40) ? -EFAULT : 0;
  }
  r[15] = 0;
  struct ifnet *ifp = if_byname(r);
  if (!ifp)
    return -ENODEV;
  struct ifaddr4 *a = first_addr(ifp);
  uint32_t ip, mask;
  uint16_t f16;
  switch (cmd) {
  case SIOCSIFFLAGS:
    memcpy(&f16, r + 16, 2);
    return if_setflags(ifp, f16, lx);
  case SIOCSIFADDR: {
    if (!sa_get4(r + 16, &ip))
      return -EAFNOSUPPORT;
    long err = if_setaddr(ifp, ip);
    if (!err)
      dhcp_stop(ifp);
    return err;
  }
  case SIOCSIFNETMASK:
    if (!sa_get4(r + 16, &mask))
      return -EAFNOSUPPORT;
    return a ? ifa_add(ifp, a->addr, mask) : -EADDRNOTAVAIL;
  case SIOCSIFBRDADDR: /* (always address | ~mask) */
    if (!sa_get4(r + 16, &ip))
      return -EAFNOSUPPORT;
    if (!a)
      return -EADDRNOTAVAIL;
    return ip == (a->addr | ~a->mask) ? 0 : -EOPNOTSUPP;
  case SIOCSIFDSTADDR:
    return -EINVAL; /* (no point-to-point interfaces) */
  case SIOCSIFMETRIC:
    return if_setmetric(ifp, n);
  case SIOCSIFMTU:
    if (n < 68 || (unsigned)n > ifp->if_hardmtu)
      return -EINVAL;
    ifp->if_mtu = (unsigned)n;
    return 0;
  case SIOCAIFADDR: {
    if (!sa_get4(r + 16, &ip))
      return -EAFNOSUPPORT;
    if (!addr_ok(ifp, ip))
      return -EINVAL;
    if (!sa_get4(r + 48, &mask)) {
      mask = class_mask(ip);
      for (int k = 0; k < IF_MAXADDR; k++) /* (an address it has keeps its mask) */
        if (ifp->if_addrs[k].used && ifp->if_addrs[k].addr == ip)
          mask = ifp->if_addrs[k].mask;
    }
    long err = ifa_add(ifp, ip, mask);
    if (!err)
      dhcp_stop(ifp);
    return err;
  }
  case SIOCDIFADDR:
    return sa_get4(r + 16, &ip) ? ifa_remove(ifp, ip) : -EAFNOSUPPORT;
  default:
    break;
  }
  if ((cmd == SIOCGIFNETMASK || cmd == SIOCGIFBRDADDR) && sa_get4(r + 16, &ip)) { /* (of that address if it has it) */
    for (int k = 0; k < IF_MAXADDR; k++)
      if (ifp->if_addrs[k].used && ifp->if_addrs[k].addr == ip)
        a = &ifp->if_addrs[k];
  }
  memset(r + 16, 0, 24);
  if (!a && (cmd == SIOCGIFADDR || cmd == SIOCGIFNETMASK || cmd == SIOCGIFBRDADDR))
    return -EADDRNOTAVAIL;
  switch (cmd) {
  case SIOCGIFINDEX: n = ifp->if_index; memcpy(r + 16, &n, 4); break;
  case SIOCGIFFLAGS:
    f16 = (uint16_t)(lx ? linux_flags(ifp->if_flags) : ifp->if_flags);
    memcpy(r + 16, &f16, 2);
    break;
  case SIOCGIFMTU: n = (int32_t)ifp->if_mtu; memcpy(r + 16, &n, 4); break;
  case SIOCGIFMETRIC: n = ifp->if_metric; memcpy(r + 16, &n, 4); break;
  case SIOCGIFADDR: sa_put4(r + 16, a->addr); break;
  case SIOCGIFNETMASK: sa_put4(r + 16, a->mask); break;
  case SIOCGIFBRDADDR:
    if (!(ifp->if_flags & IFF_BROADCAST))
      return -EINVAL;
    sa_put4(r + 16, a->addr | ~a->mask);
    break;
  case SIOCGIFHWADDR:
    r[16] = ifp->if_ether ? 1 : 4, r[17] = 0; /* ARPHRD_ETHER, ARPHRD_LOOPBACK */
    if (ifp->if_ether)
      memcpy(r + 18, ifp->if_lladdr, 6);
    break;
  default: return -EINVAL;
  }
  return copy_to_user((void *)arg, r, 40) ? -EFAULT : 0;
}
