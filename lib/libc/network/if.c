/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

#define _DEFAULT_SOURCE
#include <errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "syscall.h"
#include "ifmsg.h"

unsigned if_nametoindex(const char *name)
{
	struct ifreq r;
	int fd, ok;
	if (strlen(name) >= IF_NAMESIZE)
		return 0;
	memset(&r, 0, sizeof r);
	strcpy(r.ifr_name, name);
	if ((fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return 0;
	ok = __sys3(SYS_ioctl, fd, SIOCGIFINDEX, &r) == 0;
	close(fd);
	return ok ? (unsigned)(unsigned short)r.ifr_index : 0;
}

/* the AF_LINK address of the interface of each RTM_IFINFO: index, name */
static int each(void (*fn)(void *, unsigned, const char *, size_t), void *arg)
{
	char *buf, *p;
	size_t len;
	if (__ifmsg_list(&buf, &len) < 0)
		return -1;
	for (p = buf; p + sizeof(struct ifmsg) <= buf + len; p += ((struct ifmsg *)p)->msglen) {
		const struct ifmsg *m = (const void *)p;
		const struct sockaddr_dl *dl;
		if (!m->msglen || m->version != IFM_VERSION || m->type != IFM_RTM_IFINFO)
			continue;
		dl = (const void *)__ifmsg_addr(m, IFM_RTA_IFP);
		if (dl && dl->sdl_family == AF_LINK)
			fn(arg, m->index, dl->sdl_data, dl->sdl_nlen);
	}
	free(buf);
	return 0;
}

struct find { unsigned index; char *name; };

static void find_name(void *arg, unsigned index, const char *name, size_t n)
{
	struct find *f = arg;
	if (index == f->index && n < IF_NAMESIZE) {
		memcpy(f->name, name, n);
		f->name[n] = 0;
		f->index = 0;
	}
}

char *if_indextoname(unsigned index, char *name)
{
	struct find f = { index, name };
	if (each(find_name, &f) < 0)
		return 0;
	if (f.index) {
		errno = ENXIO;
		return 0;
	}
	return name;
}

struct list { struct if_nameindex *v; size_t n; int err; };

static void add(void *arg, unsigned index, const char *name, size_t n)
{
	struct list *l = arg;
	struct if_nameindex *v;
	char *s;
	if (l->err || n >= IF_NAMESIZE)
		return;
	if (!(v = realloc(l->v, (l->n + 2) * sizeof *v)) || !(s = malloc(n + 1))) {
		l->v = v ? v : l->v;
		l->err = ENOBUFS;
		return;
	}
	memcpy(s, name, n);
	s[n] = 0;
	l->v = v;
	l->v[l->n].if_index = index;
	l->v[l->n].if_name = s;
	l->n++;
}

struct if_nameindex *if_nameindex(void)
{
	struct list l = { 0, 0, 0 };
	if (each(add, &l) < 0)
		return 0;
	if (!l.v && !(l.v = malloc(sizeof *l.v))) {
		errno = ENOBUFS;
		return 0;
	}
	if (l.err) {
		while (l.n--)
			free(l.v[l.n].if_name);
		free(l.v);
		errno = l.err;
		return 0;
	}
	l.v[l.n].if_index = 0;
	l.v[l.n].if_name = 0;
	return l.v;
}

void if_freenameindex(struct if_nameindex *list)
{
	struct if_nameindex *l;
	for (l = list; l->if_name; l++)
		free(l->if_name);
	free(list);
}
