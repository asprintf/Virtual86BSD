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

/* <net/route.h>: the route flags; for a program, the route ioctls'
   argument and what the routing table dump (sysctl net.route) returns.
   Shared: include/net/route.h is a link to it. */
#ifndef _NET_ROUTE_H_
#define _NET_ROUTE_H_
#ifndef _KERNEL
#include <bits/features.h>
#endif

#if defined(_KERNEL) || __EXT_VISIBLE
/* rt_flags */
#define RTF_UP 0x1      /* usable */
#define RTF_GATEWAY 0x2 /* through a gateway */
#define RTF_HOST 0x4    /* a host route (the mask is /32) */

/* sysctl: net, AF_ROUTE, 0, AF_INET (or 0), NET_RT_DUMP, 0 returns an array
   of struct rtinfo, the table in the order of its entries */
#define NET_RT_DUMP 1

/* one route of the dump; the addresses are in network byte order (the
   s_addr of a struct in_addr), ri_gateway is 0 on the link; the lowest
   ri_metric of two routes to the same place wins */
struct rtinfo {
	unsigned ri_dst;
	unsigned ri_mask;
	unsigned ri_gateway;
	int ri_flags;
	int ri_metric;
	int ri_index; /* of the interface: if_indextoname() */
};
#endif

#if !defined(_KERNEL) && __EXT_VISIBLE
#include <sys/socket.h>

/* SIOCADDRT and SIOCDELRT (<sys/sockio.h>): a route to rt_dst/rt_genmask,
   through rt_gateway if RTF_GATEWAY, else on the link of the interface
   named by rt_dev; without rt_genmask the mask is /32 for RTF_HOST and
   that of the address' class otherwise (the default route's is 0);
   rt_metric is the priority plus one (0: none, the default).  The layout
   is Linux's, so its programs can set routes too. */
struct rtentry {
	unsigned long rt_pad1;
	struct sockaddr rt_dst;
	struct sockaddr rt_gateway;
	struct sockaddr rt_genmask;
	unsigned short rt_flags;
	short rt_pad2;
	unsigned long rt_pad3;
	void *rt_pad4;
	short rt_metric;
	char *rt_dev;
	unsigned long rt_mtu;
	unsigned long rt_window;
	unsigned short rt_irtt;
};

#endif

#endif
