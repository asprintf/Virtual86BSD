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

/* getifaddrs(): the interfaces and their addresses from the kernel's
 * routing table dump (sysctl NET_RT_IFLISTL; network/ifmsg.h).  Every
 * interface has an AF_LINK entry, then one per address.  One allocation:
 * free() of the first entry frees all. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include "syscall.h"
#include "ifmsg.h"

struct ent {
	struct ifaddrs ifa;   /* first: the list is the array */
	char name[IFNAMSIZ];
	struct sockaddr_storage addr, mask, dst;
};

static void copy(struct sockaddr_storage *to, const struct sockaddr *sa)
{
	size_t n = sa->sa_len ? sa->sa_len : sizeof *sa;
	memcpy(to, sa, n > sizeof *to ? sizeof *to : n);
}

int getifaddrs(struct ifaddrs **out)
{
	char *buf, *p;
	size_t len;
	struct ent *e;
	size_t nmsg = 0, k = 0;
	char name[IFNAMSIZ] = "";
	unsigned flags = 0;

	*out = 0;
	if (__ifmsg_list(&buf, &len) < 0)
		return -1;
	for (p = buf; p + sizeof(struct ifmsg) <= buf + len && ((struct ifmsg *)p)->msglen; p += ((struct ifmsg *)p)->msglen)
		nmsg++;
	if (!nmsg || !(e = calloc(nmsg, sizeof *e))) {
		free(buf);
		if (nmsg) { errno = ENOMEM; return -1; }
		return 0;
	}
	for (p = buf; p + sizeof(struct ifmsg) <= buf + len && ((struct ifmsg *)p)->msglen; p += ((struct ifmsg *)p)->msglen) {
		const struct ifmsg *m = (const void *)p;
		const struct sockaddr *sa;
		struct ifaddrs *a = &e[k].ifa;
		if (m->version != IFM_VERSION)
			continue;
		if (m->type == IFM_RTM_IFINFO) {
			const struct sockaddr_dl *dl = (const void *)__ifmsg_addr(m, IFM_RTA_IFP);
			size_t n = dl ? dl->sdl_nlen : 0;
			if (n >= IFNAMSIZ) n = IFNAMSIZ - 1;
			if (dl) memcpy(name, dl->sdl_data, n);
			name[n] = 0;
			flags = (unsigned)m->flags;
			if (!dl)
				continue;
			copy(&e[k].addr, (const void *)dl);
			a->ifa_addr = (void *)&e[k].addr;
		} else if (m->type == IFM_RTM_NEWADDR) {
			if ((sa = __ifmsg_addr(m, IFM_RTA_IFA))) {
				copy(&e[k].addr, sa);
				a->ifa_addr = (void *)&e[k].addr;
			}
			if ((sa = __ifmsg_addr(m, IFM_RTA_NETMASK))) {
				copy(&e[k].mask, sa);
				a->ifa_netmask = (void *)&e[k].mask;
			}
			if ((sa = __ifmsg_addr(m, IFM_RTA_BRD))) {
				copy(&e[k].dst, sa);
				a->ifa_dstaddr = (void *)&e[k].dst;
			}
		} else
			continue;
		strcpy(e[k].name, name);
		a->ifa_name = e[k].name;
		a->ifa_flags = flags;
		if (k)
			e[k - 1].ifa.ifa_next = a;
		k++;
	}
	free(buf);
	if (!k) {
		free(e);
		return 0;
	}
	*out = &e[0].ifa;
	return 0;
}

void freeifaddrs(struct ifaddrs *ifa)
{
	free(ifa);
}
