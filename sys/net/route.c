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

/* route.c - the routing table: destination, netmask, gateway, interface,
   flags, metric.  The lookup is the longest prefix, then the lowest
   metric.  A route to a network on the link (gateway 0) is added with an
   interface's address (ifa_add); a route through a gateway needs that
   gateway on such a network. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <uvm/uvm.h>
#include <net/if_var.h>

struct rtentry rtable[RTMAX];

static bool rt_match(const struct rtentry *r, uint32_t dst) { return r->used && (dst & r->mask) == r->dst; }

struct rtentry *rt_lookup(uint32_t dst) {
  struct rtentry *best = NULL;
  for (int i = 0; i < RTMAX; i++) {
    struct rtentry *r = &rtable[i];
    if (!rt_match(r, dst) || !(r->ifp->if_flags & IFF_UP))
      continue;
    if (!best || r->mask > best->mask || (r->mask == best->mask && r->metric < best->metric))
      best = r;
  }
  return best;
}

/* the interface whose network (a route on the link) holds the gateway */
static struct ifnet *on_link(uint32_t gw) {
  for (int i = 0; i < RTMAX; i++)
    if (rtable[i].used && !rtable[i].gw && rt_match(&rtable[i], gw))
      return rtable[i].ifp;
  return NULL;
}

/* gw 0: a network on the link of ifp; else through gw (ifp is taken from
   the network the gateway is on, and ifp may be NULL) */
int rt_add(uint32_t dst, uint32_t mask, uint32_t gw, struct ifnet *ifp, int metric) {
  if (~mask & (~mask + 1))
    return -EINVAL;
  dst &= mask;
  for (int i = 0; i < RTMAX; i++)
    if (rtable[i].used && rtable[i].dst == dst && rtable[i].mask == mask)
      return -EEXIST;
  if (gw) {
    struct ifnet *via = on_link(gw);
    if (!via || (ifp && ifp != via))
      return -ENETUNREACH;
    ifp = via;
  } else if (!ifp)
    return -EINVAL;
  for (int i = 0; i < RTMAX; i++) {
    struct rtentry *r = &rtable[i];
    if (r->used)
      continue;
    r->used = true;
    r->dst = dst;
    r->mask = mask;
    r->gw = gw;
    r->ifp = ifp;
    r->metric = metric;
    r->flags = RTF_UP | (gw ? RTF_GATEWAY : 0) | (mask == 0xffffffffU ? RTF_HOST : 0);
    return 0;
  }
  return -ENOBUFS;
}

int rt_delete(uint32_t dst, uint32_t mask) {
  dst &= mask;
  for (int i = 0; i < RTMAX; i++)
    if (rtable[i].used && rtable[i].dst == dst && rtable[i].mask == mask) {
      rtable[i].used = false;
      return 0;
    }
  return -ESRCH;
}

void rt_prune(void) {
  for (bool again = true; again;) {
    again = false;
    for (int i = 0; i < RTMAX; i++)
      if (rtable[i].used && rtable[i].gw && on_link(rtable[i].gw) != rtable[i].ifp) {
        rtable[i].used = false;
        again = true;
      }
  }
}

/* ---- the route ioctls and the dump ---- */

/* SIOCADDRT/SIOCDELRT: struct rtentry of <net/route.h> (Linux's layout):
   rt_dst at 8, rt_gateway 24, rt_genmask 40, rt_flags 56, rt_metric 80,
   rt_dev 88 */
long rtioctl(unsigned cmd, uint64_t arg) {
  char r[120];
  uint32_t dst, gw = 0, mask;
  uint16_t flags;
  int16_t metric;
  uint64_t udev;
  if (!capable())
    return -EPERM;
  if (cmd != SIOCADDRT && cmd != SIOCDELRT)
    return -EINVAL;
  if (copy_from_user(r, (void *)arg, sizeof r))
    return -EFAULT;
  memcpy(&flags, r + 56, 2);
  memcpy(&metric, r + 80, 2);
  memcpy(&udev, r + 88, 8);
  if (!sa_get4(r + 8, &dst))
    return -EAFNOSUPPORT;
  if (r[40] || r[41]) { /* a mask */
    if (!sa_get4(r + 40, &mask))
      return -EAFNOSUPPORT;
  } else {
    mask = (flags & RTF_HOST) ? 0xffffffffU : !dst ? 0 : dst < 0x80000000U ? 0xff000000U : dst < 0xc0000000U ? 0xffff0000U : 0xffffff00U;
  }
  if ((flags & RTF_HOST) && mask != 0xffffffffU)
    return -EINVAL;
  if (cmd == SIOCDELRT) {
    int err = rt_delete(dst, mask);
    rt_prune(); /* (what went through its network went with it) */
    return err;
  }
  if (flags & RTF_GATEWAY) {
    if (!sa_get4(r + 24, &gw) || !gw)
      return -EINVAL;
  }
  struct ifnet *ifp = NULL;
  if (udev) {
    char name[IFNAMSIZ];
    long len = strncpy_from_user(name, (const char *)udev, sizeof name);
    if (len == -ENAMETOOLONG)
      return -ENODEV;
    if (len < 0)
      return -EFAULT;
    if (!(ifp = if_byname(name)))
      return -ENODEV;
  }
  if (!gw && !ifp)
    return -ENODEV; /* on the link of which interface? */
  return rt_add(dst, mask, gw, ifp, metric > 0 ? metric - 1 : 0);
}

static unsigned be32(uint32_t v) { return v >> 24 | (v >> 8 & 0xff00U) | (v << 8 & 0xff0000U) | v << 24; }

/* the table as struct rtinfo (network byte order addresses), at most max bytes */
int rt_dump(void *buf, int max) {
  struct rtinfo *o = buf;
  int n = 0;
  for (int i = 0; i < RTMAX && (int)((n + 1) * sizeof *o) <= max; i++) {
    const struct rtentry *r = &rtable[i];
    if (!r->used)
      continue;
    o[n].ri_dst = be32(r->dst);
    o[n].ri_mask = be32(r->mask);
    o[n].ri_gateway = be32(r->gw);
    o[n].ri_flags = (int)r->flags;
    o[n].ri_metric = r->metric;
    o[n].ri_index = r->ifp->if_index;
    n++;
  }
  return (int)(n * sizeof *o);
}
