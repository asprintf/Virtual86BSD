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

/* <net/route.h>: the routing socket (socket(PF_ROUTE, SOCK_RAW, 0)) messages and the
   route flags, FreeBSD's (sys/net/route.h, the part for programs). */
#ifndef _NET_ROUTE_H_
#define _NET_ROUTE_H_
#include <bits/features.h>

#if __EXT_VISIBLE
#include <sys/types.h>
#include <sys/socket.h>

struct rt_metrics {
	unsigned long rmx_locks;	/* the kernel must leave these values alone */
	unsigned long rmx_mtu;
	unsigned long rmx_hopcount;
	unsigned long rmx_expire;
	unsigned long rmx_recvpipe;
	unsigned long rmx_sendpipe;
	unsigned long rmx_ssthresh;
	unsigned long rmx_rtt;
	unsigned long rmx_rttvar;
	unsigned long rmx_pksent;
	unsigned long rmx_weight;
	unsigned long rmx_nhidx;
	unsigned long rmx_filler[2];
};

#define RTM_VERSION 5

struct rt_msghdr {
	unsigned short rtm_msglen;	/* the length of the message, the addresses included */
	unsigned char rtm_version;
	unsigned char rtm_type;
	unsigned short rtm_index;	/* of the interface */
	unsigned short _rtm_spare1;
	int rtm_flags;
	int rtm_addrs;			/* RTA_*: which addresses follow */
	pid_t rtm_pid;
	int rtm_seq;
	int rtm_errno;
	int rtm_fmask;
	unsigned long rtm_inits;
	struct rt_metrics rtm_rmx;
};

/* rtm_type */
#define RTM_ADD 0x1
#define RTM_DELETE 0x2
#define RTM_CHANGE 0x3
#define RTM_GET 0x4
#define RTM_LOSING 0x5
#define RTM_REDIRECT 0x6
#define RTM_MISS 0x7

/* rt_flags */
#define RTF_UP 0x1
#define RTF_GATEWAY 0x2
#define RTF_HOST 0x4
#define RTF_REJECT 0x8
#define RTF_DYNAMIC 0x10
#define RTF_MODIFIED 0x20
#define RTF_DONE 0x40
#define RTF_XRESOLVE 0x200
#define RTF_LLINFO 0x400
#define RTF_STATIC 0x800
#define RTF_BLACKHOLE 0x1000
#define RTF_PROTO2 0x4000
#define RTF_PROTO1 0x8000
#define RTF_LOCAL 0x200000
#define RTF_BROADCAST 0x400000
#define RTF_MULTICAST 0x800000

/* rtm_addrs, and the index (RTAX_*) of an address in the message */
#define RTA_DST 0x1
#define RTA_GATEWAY 0x2
#define RTA_NETMASK 0x4
#define RTA_GENMASK 0x8
#define RTA_IFP 0x10
#define RTA_IFA 0x20
#define RTA_AUTHOR 0x40
#define RTA_BRD 0x80
#define RTAX_DST 0
#define RTAX_GATEWAY 1
#define RTAX_NETMASK 2
#define RTAX_GENMASK 3
#define RTAX_IFP 4
#define RTAX_IFA 5
#define RTAX_AUTHOR 6
#define RTAX_BRD 7
#define RTAX_MAX 8

/* the addresses of a message are struct sockaddrs, each padded to a multiple of a long */
#define SA_SIZE(sa) ((sa)->sa_len ? (1 + (((sa)->sa_len - 1) | (sizeof(long) - 1))) : sizeof(long))

/* sysctl: CTL_NET, PF_ROUTE, 0, family (0: all), NET_RT_*, flags */
#define NET_RT_DUMP 1
#define NET_RT_FLAGS 2
#define NET_RT_IFLIST 3
#endif

#endif
