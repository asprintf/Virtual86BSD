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
