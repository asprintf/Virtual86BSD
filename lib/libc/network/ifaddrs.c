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

/* getifaddrs(3): the interfaces and their addresses, from the interface
   ioctls (<sys/sockio.h>): the names by index, SIOCGIFCONF for the IPv4
   addresses, the mask and broadcast address of each by SIOCGIFNETMASK and
   SIOCGIFBRDADDR (the address picks the entry).  Every interface has an
   AF_LINK entry, then one per address.  One allocation: free() of the
   first entry frees all. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

struct ent {
	struct ifaddrs ifa;   /* first: the list is the array */
	char name[IFNAMSIZ];
	struct sockaddr_storage addr, mask, dst;
};

#define MAXIF 64

int getifaddrs(struct ifaddrs **out)
{
	struct ifreq r, *conf = 0;
	struct ifconf ic;
	struct ent *e = 0;
	char names[MAXIF][IFNAMSIZ];
	unsigned flags[MAXIF];
	unsigned char hw[MAXIF][6];
	int fd, nif = 0, naddr = 0, n, i, k, err = 0, tries = 0;

	if ((fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return -1;
	for (i = 1; i <= MAXIF; i++) {
		memset(&r, 0, sizeof r);
		r.ifr_index = i;
		if (ioctl(fd, SIOCGIFNAME, &r) < 0)
			break;
		memcpy(names[nif], r.ifr_name, IFNAMSIZ);
		names[nif][IFNAMSIZ - 1] = 0;
		if (ioctl(fd, SIOCGIFFLAGS, &r) < 0)
			goto fail;
		flags[nif] = (unsigned short)r.ifr_flags;
		memset(hw[nif], 0, 6);
		if (ioctl(fd, SIOCGIFHWADDR, &r) == 0 && r.ifr_hwaddr.sa_family == 1)
			memcpy(hw[nif], r.ifr_hwaddr.sa_data, 6);
		nif++;
	}
	/* the addresses; the table can grow between the calls */
	do {
		free(conf);
		ic.ifc_len = 0;
		ic.ifc_buf = 0;
		if (ioctl(fd, SIOCGIFCONF, &ic) < 0)
			goto fail;
		n = ic.ifc_len;
		if (!(conf = malloc((size_t)n + sizeof *conf)))
			goto nomem;
		ic.ifc_len = n;
		ic.ifc_req = conf;
		if (ioctl(fd, SIOCGIFCONF, &ic) < 0)
			goto fail;
	} while (ic.ifc_len > n && ++tries < 5);
	naddr = (int)(ic.ifc_len > n ? n : ic.ifc_len) / (int)sizeof *conf;

	if (!(e = calloc((size_t)(nif + naddr), sizeof *e)))
		goto nomem;
	k = 0;
	for (i = 0; i < nif; i++) {
		struct sockaddr_dl *dl = (struct sockaddr_dl *)&e[k].addr;
		struct ifaddrs *a = &e[k].ifa;
		memcpy(e[k].name, names[i], IFNAMSIZ);
		a->ifa_name = e[k].name;
		a->ifa_flags = flags[i];
		a->ifa_addr = (struct sockaddr *)dl;
		dl->sdl_family = AF_LINK;
		dl->sdl_index = (unsigned short)(i + 1);
		dl->sdl_type = (flags[i] & 0x8) ? IFT_LOOP : IFT_ETHER; /* IFF_LOOPBACK */
		dl->sdl_alen = (flags[i] & 0x8) ? 0 : 6;
		memcpy(dl->sdl_data, hw[i], 6);
		a->ifa_next = &e[k + 1].ifa;
		k++;
		for (int j = 0; j < naddr; j++) {
			if (strncmp(conf[j].ifr_name, names[i], IFNAMSIZ))
				continue;
			if (k >= nif + naddr)
				break;
			a = &e[k].ifa;
			memcpy(e[k].name, names[i], IFNAMSIZ);
			a->ifa_name = e[k].name;
			a->ifa_flags = flags[i];
			memcpy(&e[k].addr, &conf[j].ifr_addr, sizeof conf[j].ifr_addr);
			a->ifa_addr = (struct sockaddr *)&e[k].addr;
			r = conf[j]; /* (the address selects the answer) */
			if (ioctl(fd, SIOCGIFNETMASK, &r) == 0) {
				memcpy(&e[k].mask, &r.ifr_addr, sizeof r.ifr_addr);
				a->ifa_netmask = (struct sockaddr *)&e[k].mask;
			}
			r = conf[j];
			if (ioctl(fd, SIOCGIFBRDADDR, &r) == 0) {
				memcpy(&e[k].dst, &r.ifr_broadaddr, sizeof r.ifr_broadaddr);
				a->ifa_dstaddr = (struct sockaddr *)&e[k].dst;
			}
			a->ifa_next = &e[k + 1].ifa;
			k++;
		}
	}
	if (k == 0) {
		free(e);
		e = 0;
	} else
		e[k - 1].ifa.ifa_next = 0;
	free(conf);
	close(fd);
	*out = e ? &e[0].ifa : 0;
	return 0;
nomem:
	errno = ENOMEM;
fail:
	err = errno;
	free(conf);
	free(e);
	close(fd);
	errno = err;
	return -1;
}

void freeifaddrs(struct ifaddrs *ifa)
{
	free(ifa);
}
