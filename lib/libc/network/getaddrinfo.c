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

/* getaddrinfo: numeric addresses, then /etc/hosts, then the DNS; the
 * services from /etc/services or numbers.  As glibc: without a socket
 * type, an address comes as a stream, a datagram and a raw socket (those
 * the service exists for); the addresses are sorted by the rules of RFC
 * 6724 (with the default policy table of glibc), which ask the kernel
 * for the source address it would use for each. */
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netdb_impl.h"

#define FLAGS (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV | AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG)

void freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;
	for (; ai; ai = next) {
		next = ai->ai_next;
		free(ai->ai_canonname);
		free(ai);
	}
}

/* ---- services ---- */
static int services(struct __service *s, const char *name, int socktype, int proto, int flags)
{
	static const struct __service all[3] = {
		{0, IPPROTO_TCP, SOCK_STREAM}, {0, IPPROTO_UDP, SOCK_DGRAM}, {0, 0, SOCK_RAW}};
	int i, n = 0, numeric = 0;
	unsigned long port = 0;
	char *end;

	if (name) {
		if (*name >= '0' && *name <= '9') {
			port = strtoul(name, &end, 10);
			numeric = !*end;
		}
		if (!numeric && (flags & AI_NUMERICSERV))
			return EAI_NONAME;
	}
	for (i = 0; i < 3; i++) {
		struct __service t = all[i];
		if (socktype && socktype != t.socktype)
			continue;
		if (proto && t.proto && proto != t.proto)
			continue;
		if (!socktype && proto && !t.proto)
			continue;           /* raw only when asked for (as glibc) */
		if (proto && !t.proto)
			t.proto = (unsigned char)proto;
		if (name && !numeric) {
			int p;
			if (t.socktype == SOCK_RAW)
				continue;
			p = __services_lookup(name, t.proto == IPPROTO_UDP ? "udp" : "tcp");
			if (p < 0)
				continue;
			t.port = (unsigned short)p;
		} else
			t.port = (unsigned short)port;
		s[n++] = t;
	}
	if (!n)
		return name && !numeric ? EAI_SERVICE : EAI_SOCKTYPE;
	return n;
}

/* ---- names ---- */
static int numeric(struct __address *a, const char *name, int family)
{
	struct in_addr v4;
	char buf[64], *pct;
	memset(a, 0, sizeof *a);
	if (__inet_aton(name, &v4)) {
		a->family = AF_INET;
		memcpy(a->addr, &v4, 4);
		return 1;
	}
	if (strlen(name) >= sizeof buf)
		return 0;
	strcpy(buf, name);
	if ((pct = strchr(buf, '%')))
		*pct++ = 0;
	if (inet_pton(AF_INET6, buf, a->addr) != 1)
		return 0;
	a->family = AF_INET6;
	if (pct) {
		char *end;
		unsigned long s = strtoul(pct, &end, 10);
		if (*end || !*pct)
			s = if_nametoindex(pct);
		if (!s)
			return 2;       /* an IPv6 address, an unknown scope */
		a->scope = (unsigned)s;
	}
	(void)family;
	return 1;
}

/* does this host have a non-loopback address of the family?  (A UDP
 * "connection" to a global address sends nothing.) */
static int configured(int family)
{
	int fd = socket(family, SOCK_DGRAM | SOCK_CLOEXEC, 0), ok;
	if (fd < 0)
		return 0;
	if (family == AF_INET) {
		struct sockaddr_in sa = {AF_INET, htons(53), {htonl(0x08080808)}, {0}};
		ok = connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0;
	} else {
		struct sockaddr_in6 sa;
		memset(&sa, 0, sizeof sa);
		sa.sin6_family = AF_INET6;
		sa.sin6_port = htons(53);
		sa.sin6_addr.s6_addr[0] = 0x20;
		sa.sin6_addr.s6_addr[1] = 0x01;
		sa.sin6_addr.s6_addr[2] = 0x48;
		sa.sin6_addr.s6_addr[3] = 0x60;
		sa.sin6_addr.s6_addr[15] = 0x88;
		ok = connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0;
	}
	close(fd);
	return ok;
}

/* ---- sorting (RFC 6724) ---- */
static const struct policy {
	unsigned char addr[16], len, prec, label;
} policy[] = {
	{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 128, 50, 0},
	{{0x20, 0x02}, 16, 30, 2},
	{{0}, 96, 20, 3},
	{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}, 96, 10, 4},
	{{0xfe, 0xc0}, 10, 1, 5},
	{{0xfc}, 7, 1, 6},
	{{0x20, 0x01}, 32, 1, 7},
	{{0}, 0, 40, 1},
};

static int prefix_match(const unsigned char *a, const unsigned char *p, int len)
{
	int i;
	for (i = 0; i < len / 8; i++)
		if (a[i] != p[i])
			return 0;
	return !(len % 8) || !((a[i] ^ p[i]) & (0xff00 >> len % 8));
}

static const struct policy *policy_of(const unsigned char *a)
{
	const struct policy *p = policy;
	while (!prefix_match(a, p->addr, p->len))
		p++;
	return p;
}

static int scope_of(const unsigned char *a)
{
	if (a[0] == 0xff)
		return a[1] & 15;
	if (a[0] == 0xfe && (a[1] & 0xc0) == 0x80)
		return 2;
	if (IN6_IS_ADDR_LOOPBACK(a))
		return 2;
	if (a[0] == 0xfe && (a[1] & 0xc0) == 0xc0)
		return 5;
	if (IN6_IS_ADDR_V4MAPPED(a)) {
		if (a[12] == 127 || (a[12] == 169 && a[13] == 254))
			return 2;
		return 14;
	}
	return 14;
}

static int common_prefix(const unsigned char *a, const unsigned char *b)
{
	int i, n = 0;
	for (i = 0; i < 16 && a[i] == b[i]; i++)
		n += 8;
	if (i < 16) {
		unsigned x = a[i] ^ b[i];
		while (!(x & 0x80)) {
			n++;
			x <<= 1;
		}
	}
	return n;
}

struct sortable {
	struct __address a;
	unsigned char d[16], s[16];
	int usable, index;
};

static int cmp(const void *x, const void *y)
{
	const struct sortable *a = x, *b = y;
	int as, bs, d;
	/* rule 1: unusable destinations last */
	if (a->usable != b->usable)
		return b->usable - a->usable;
	if (a->usable) {
		/* rule 2: matching scope */
		as = scope_of(a->d) == scope_of(a->s);
		bs = scope_of(b->d) == scope_of(b->s);
		if (as != bs)
			return bs - as;
		/* rule 5: matching label */
		as = policy_of(a->d)->label == policy_of(a->s)->label;
		bs = policy_of(b->d)->label == policy_of(b->s)->label;
		if (as != bs)
			return bs - as;
	}
	/* rule 6: higher precedence */
	if ((d = policy_of(b->d)->prec - policy_of(a->d)->prec))
		return d;
	/* rule 8: smaller scope */
	if ((d = scope_of(a->d) - scope_of(b->d)))
		return d;
	/* rule 9: longest matching prefix (same family) */
	if (a->usable && b->usable && a->a.family == AF_INET6 && b->a.family == AF_INET6) {
		if ((d = common_prefix(b->d, b->s) - common_prefix(a->d, a->s)))
			return d;
	}
	/* rule 10: as found */
	return a->index - b->index;
}

static void to6(unsigned char *d, const struct __address *a)
{
	if (a->family == AF_INET6)
		memcpy(d, a->addr, 16);
	else {
		memset(d, 0, 10);
		d[10] = d[11] = 0xff;
		memcpy(d + 12, a->addr, 4);
	}
}

static void sort(struct __address *buf, int n)
{
	struct sortable s[MAXADDRS];
	int i;
	if (n < 2)
		return;
	for (i = 0; i < n; i++) {
		int fd;
		s[i].a = buf[i];
		s[i].index = i;
		s[i].usable = 0;
		to6(s[i].d, &buf[i]);
		memset(s[i].s, 0, 16);
		fd = socket(buf[i].family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
		if (fd >= 0) {
			union { struct sockaddr_in v4; struct sockaddr_in6 v6; } da, sa;
			socklen_t len, sl = sizeof sa;
			memset(&da, 0, sizeof da);
			if (buf[i].family == AF_INET) {
				da.v4.sin_family = AF_INET;
				da.v4.sin_port = htons(65535);
				memcpy(&da.v4.sin_addr, buf[i].addr, 4);
				len = sizeof da.v4;
			} else {
				da.v6.sin6_family = AF_INET6;
				da.v6.sin6_port = htons(65535);
				memcpy(&da.v6.sin6_addr, buf[i].addr, 16);
				da.v6.sin6_scope_id = buf[i].scope;
				len = sizeof da.v6;
			}
			if (connect(fd, (struct sockaddr *)&da, len) == 0 &&
			    getsockname(fd, (struct sockaddr *)&sa, &sl) == 0) {
				struct __address src;
				memset(&src, 0, sizeof src);
				src.family = buf[i].family;
				if (src.family == AF_INET)
					memcpy(src.addr, &sa.v4.sin_addr, 4);
				else
					memcpy(src.addr, &sa.v6.sin6_addr, 16);
				to6(s[i].s, &src);
				s[i].usable = 1;
			}
			close(fd);
		}
	}
	qsort(s, (size_t)n, sizeof *s, cmp);
	for (i = 0; i < n; i++)
		buf[i] = s[i].a;
}

/* the addresses of the name: their number, or an EAI_ error */
static int names(struct __address *buf, char canon[256], const char *name, int family, int flags)
{
	int n, i, j;
	*canon = 0;
	if (!name) {
		/* the loopback or wildcard addresses: IPv4 first, then IPv6 */
		n = 0;
		if (family != AF_INET6) {
			memset(&buf[n], 0, sizeof buf[n]);
			buf[n].family = AF_INET;
			if (!(flags & AI_PASSIVE))
				buf[n].addr[0] = 127, buf[n].addr[3] = 1;
			n++;
		}
		if (family != AF_INET) {
			memset(&buf[n], 0, sizeof buf[n]);
			buf[n].family = AF_INET6;
			if (!(flags & AI_PASSIVE))
				buf[n].addr[15] = 1;
			n++;
		}
		return n;
	}
	if ((n = numeric(&buf[0], name, family))) {
		strncpy(canon, name, 255);
		canon[255] = 0;
		/* a v4-mapped address for AF_INET: the IPv4 one (as glibc) */
		if (family == AF_INET && buf[0].family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(buf[0].addr)) {
			memmove(buf[0].addr, buf[0].addr + 12, 4);
			memset(buf[0].addr + 4, 0, 12);
			buf[0].family = AF_INET;
			buf[0].scope = 0;
		}
		if (family != AF_UNSPEC && buf[0].family != family &&
		    !(family == AF_INET6 && (flags & AI_V4MAPPED)))     /* mapped below */
			return -9;              /* EAI_ADDRFAMILY */
		return n == 2 ? EAI_NONAME : 1;
	}
	if (flags & AI_NUMERICHOST)
		return EAI_NONAME;
	n = __hosts_lookup(buf, MAXADDRS, canon, name, family == AF_INET6 && (flags & AI_V4MAPPED) ? AF_UNSPEC : family);
	if (n <= 0)
		n = __dns_lookup(buf, MAXADDRS, canon, name,
		                 family == AF_INET6 && (flags & AI_V4MAPPED) ? AF_UNSPEC : family);
	if (n <= 0)
		return n;
	/* with AF_INET6 and AI_V4MAPPED: IPv4 ones only if there are no
	 * IPv6 ones, or AI_ALL (mapping: below) */
	if (family == AF_INET6 && (flags & AI_V4MAPPED) && !(flags & AI_ALL)) {
		int v6 = 0;
		for (i = 0; i < n; i++)
			v6 |= buf[i].family == AF_INET6;
		if (v6) {
			for (i = j = 0; i < n; i++)
				if (buf[i].family == AF_INET6)
					buf[j++] = buf[i];
			n = j;
		}
	}
	return n;
}

int getaddrinfo(const char *__RESTRICT node, const char *__RESTRICT service, const struct addrinfo *__RESTRICT hints,
                struct addrinfo **__RESTRICT res)
{
	struct __address addrs[MAXADDRS];
	struct __service servs[MAXSERVS];
	char canon[256];
	int family = AF_UNSPEC, socktype = 0, proto = 0, flags = 0, na, ns, i, j, k;
	struct addrinfo *head = 0, **tail = &head;

	if (hints) {
		family = hints->ai_family;
		socktype = hints->ai_socktype;
		proto = hints->ai_protocol;
		flags = hints->ai_flags;
		if (flags & ~FLAGS)
			return EAI_BADFLAGS;
		if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6)
			return EAI_FAMILY;
		if (socktype && socktype != SOCK_STREAM && socktype != SOCK_DGRAM && socktype != SOCK_RAW)
			return EAI_SOCKTYPE;
	}
	if (!node && !service)
		return EAI_NONAME;
	if ((flags & AI_CANONNAME) && !node)
		return EAI_BADFLAGS;
	if (socktype == SOCK_RAW && service)
		return EAI_SERVICE;     /* as glibc */
	if ((ns = services(servs, service, socktype, proto, flags)) < 0)
		return ns;
	if (flags & AI_ADDRCONFIG) {
		/* only the families of which the host has (non-loopback)
		 * addresses, as glibc; with neither (only a loopback),
		 * AF_UNSPEC is not narrowed, as glibc */
		int v4 = configured(AF_INET), v6 = configured(AF_INET6);
		if (family == AF_UNSPEC) {
			if (v4 && !v6) family = AF_INET;
			else if (v6 && !v4) family = AF_INET6;
		} else if ((family == AF_INET && !v4) || (family == AF_INET6 && !v6))
			return EAI_NONAME;
	}
	na = names(addrs, canon, node, family, flags);
	if (na < 0)
		return na;
	if (!na)
		return EAI_NONAME;
	if (family == AF_INET6 && (flags & AI_V4MAPPED)) {
		for (i = 0; i < na; i++)
			if (addrs[i].family == AF_INET) {
				unsigned char v4[4];
				memcpy(v4, addrs[i].addr, 4);
				memset(addrs[i].addr, 0, 16);
				addrs[i].addr[10] = addrs[i].addr[11] = 0xff;
				memcpy(addrs[i].addr + 12, v4, 4);
				addrs[i].family = AF_INET6;
			}
	}
	if (node && na > 1)
		sort(addrs, na);

	for (i = 0, k = 0; i < na; i++)
		for (j = 0; j < ns; j++, k++) {
			struct addrinfo *ai;
			size_t sl = addrs[i].family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
			ai = calloc(1, sizeof *ai + sizeof(struct sockaddr_in6));
			if (!ai)
				goto nomem;
			ai->ai_family = addrs[i].family;
			ai->ai_socktype = servs[j].socktype;
			ai->ai_protocol = servs[j].proto;
			ai->ai_addrlen = (socklen_t)sl;
			ai->ai_addr = (struct sockaddr *)(ai + 1);
			if (addrs[i].family == AF_INET) {
				struct sockaddr_in *sa = (struct sockaddr_in *)ai->ai_addr;
				sa->sin_family = AF_INET;
				sa->sin_port = htons(servs[j].port);
				memcpy(&sa->sin_addr, addrs[i].addr, 4);
			} else {
				struct sockaddr_in6 *sa = (struct sockaddr_in6 *)ai->ai_addr;
				sa->sin6_family = AF_INET6;
				sa->sin6_port = htons(servs[j].port);
				memcpy(&sa->sin6_addr, addrs[i].addr, 16);
				sa->sin6_scope_id = addrs[i].scope;
			}
			if (!k && (flags & AI_CANONNAME) && node) {
				if (!(ai->ai_canonname = strdup(*canon ? canon : node))) {
					free(ai);
					goto nomem;
				}
			}
			*tail = ai;
			tail = &ai->ai_next;
		}
	*res = head;
	return 0;
nomem:
	freeaddrinfo(head);
	return EAI_MEMORY;
}

static const char *const msgs[] = {
	"Success",
	"Bad value for ai_flags",
	"Name or service not known",
	"Temporary failure in name resolution",
	"Non-recoverable failure in name resolution",
	"No address associated with hostname",
	"ai_family not supported",
	"ai_socktype not supported",
	"Servname not supported for ai_socktype",
	"Address family for hostname not supported",
	"Memory allocation failure",
	"System error",
	"Result too large for supplied buffer",
};

const char *gai_strerror(int e)
{
	if (e > 0 || e < -12)
		return "Unknown error";
	return msgs[-e];
}
