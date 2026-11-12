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

/* <net/if.h>: for a program, the interface names and indexes (POSIX) and,
   not POSIX, the interface flags and the structures of the interface
   ioctls (<sys/sockio.h>) with the BSDs' names; for the kernel, the flags
   (net/if_var.h has the rest).  Shared: include/net/if.h is a link to it. */
#ifndef _NET_IF_H
#define _NET_IF_H
#ifndef _KERNEL
#include <bits/features.h>
#endif

#define IF_NAMESIZE 16

#if defined(_KERNEL) || __EXT_VISIBLE
#define IFNAMSIZ IF_NAMESIZE

/* ifr_flags, ifnet.if_flags */
#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_DEBUG 0x4
#define IFF_LOOPBACK 0x8
#define IFF_POINTOPOINT 0x10
#define IFF_STATICARP 0x20
#define IFF_RUNNING 0x40
#define IFF_NOARP 0x80
#define IFF_PROMISC 0x100
#define IFF_ALLMULTI 0x200
#define IFF_OACTIVE 0x400
#define IFF_SIMPLEX 0x800
#define IFF_LINK0 0x1000
#define IFF_LINK1 0x2000
#define IFF_LINK2 0x4000
#define IFF_MULTICAST 0x8000
/* the ones a program cannot change */
#define IFF_CANTCHANGE (IFF_BROADCAST | IFF_LOOPBACK | IFF_POINTOPOINT | IFF_RUNNING | IFF_OACTIVE | IFF_SIMPLEX | IFF_MULTICAST)
#endif

#ifndef _KERNEL
#if __EXT_VISIBLE
#include <sys/socket.h>

/* the argument of most interface ioctls: the name, then what is asked or
   given (40 bytes in all) */
struct ifreq {
	char ifr_name[IFNAMSIZ];
	union {
		struct sockaddr ifru_addr;
		struct sockaddr ifru_dstaddr;
		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_hwaddr; /* sa_family 1 (ARPHRD_ETHER), sa_data the address */
		short ifru_flags;
		int ifru_metric;
		int ifru_mtu;
		int ifru_index;
		void *ifru_data;
		char ifru_pad[24];
	} ifr_ifru;
};
#define ifr_addr ifr_ifru.ifru_addr
#define ifr_dstaddr ifr_ifru.ifru_dstaddr
#define ifr_broadaddr ifr_ifru.ifru_broadaddr
#define ifr_hwaddr ifr_ifru.ifru_hwaddr
#define ifr_flags ifr_ifru.ifru_flags
#define ifr_metric ifr_ifru.ifru_metric
#define ifr_mtu ifr_ifru.ifru_mtu
#define ifr_index ifr_ifru.ifru_index
#define ifr_data ifr_ifru.ifru_data

/* SIOCAIFADDR: add an address with its mask (and broadcast address) */
struct ifaliasreq {
	char ifra_name[IFNAMSIZ];
	struct sockaddr ifra_addr;
	struct sockaddr ifra_dstaddr;
	struct sockaddr ifra_mask;
};
#define ifra_broadaddr ifra_dstaddr

/* SIOCGIFCONF: ifc_len is the size of the buffer, then what was filled in */
struct ifconf {
	int ifc_len;
	union {
		char *ifcu_buf;
		struct ifreq *ifcu_req;
	} ifc_ifcu;
};
#define ifc_buf ifc_ifcu.ifcu_buf
#define ifc_req ifc_ifcu.ifcu_req
#endif

struct if_nameindex {
	unsigned if_index;
	char *if_name;
};

__BEGIN_DECLS
void if_freenameindex(struct if_nameindex *);
char *if_indextoname(unsigned, char *);
struct if_nameindex *if_nameindex(void);
unsigned if_nametoindex(const char *);
__END_DECLS
#endif

#endif
