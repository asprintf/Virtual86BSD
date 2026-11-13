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

/* gethostbyname, gethostbyaddr, h_errno: SUSv2 (XNS), POSIX.1-2001 */
#define _DEFAULT_SOURCE
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

int h_errno;

#define MAXADDR 32

static struct {
	struct hostent h;
	char name[NI_MAXHOST];
	char alias[NI_MAXHOST];
	char *aliases[2];
	unsigned char addr[MAXADDR][16];
	char *addrs[MAXADDR + 1];
} r;

static int herr(int eai)
{
	switch (eai) {
	case EAI_NONAME: return HOST_NOT_FOUND;
	case EAI_NODATA: case EAI_ADDRFAMILY: return NO_DATA;
	case EAI_AGAIN: return TRY_AGAIN;
	default: return NO_RECOVERY;
	}
}

struct hostent *gethostbyname(const char *name)
{
	struct addrinfo hint, *res, *p;
	int e, n = 0, i;
	memset(&hint, 0, sizeof hint);
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_CANONNAME;
	if ((e = getaddrinfo(name, 0, &hint, &res))) {
		h_errno = herr(e);
		return 0;
	}
	memset(&r, 0, sizeof r);
	for (p = res; p && n < MAXADDR; p = p->ai_next) {
		const struct in_addr *a = &((const struct sockaddr_in *)p->ai_addr)->sin_addr;
		for (i = 0; i < n && memcmp(r.addr[i], a, 4); i++)
			;
		if (i < n)
			continue;
		memcpy(r.addr[n], a, 4);
		r.addrs[n] = (char *)r.addr[n];
		n++;
	}
	strncpy(r.name, res->ai_canonname ? res->ai_canonname : name, sizeof r.name - 1);
	freeaddrinfo(res);
	if (strcmp(r.name, name)) {
		strncpy(r.alias, name, sizeof r.alias - 1);
		r.aliases[0] = r.alias;
	}
	r.h.h_name = r.name;
	r.h.h_aliases = r.aliases;
	r.h.h_addrtype = AF_INET;
	r.h.h_length = 4;
	r.h.h_addr_list = r.addrs;
	return &r.h;
}

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
{
	struct sockaddr_in s4;
	struct sockaddr_in6 s6;
	const struct sockaddr *sa;
	socklen_t sl;
	int e;
	if (type == AF_INET && len == 4) {
		memset(&s4, 0, sizeof s4);
		s4.sin_family = AF_INET;
		memcpy(&s4.sin_addr, addr, 4);
		sa = (const struct sockaddr *)&s4;
		sl = sizeof s4;
	} else if (type == AF_INET6 && len == 16) {
		memset(&s6, 0, sizeof s6);
		s6.sin6_family = AF_INET6;
		memcpy(&s6.sin6_addr, addr, 16);
		sa = (const struct sockaddr *)&s6;
		sl = sizeof s6;
	} else {
		h_errno = NO_RECOVERY;
		return 0;
	}
	memset(&r, 0, sizeof r);
	if ((e = getnameinfo(sa, sl, r.name, sizeof r.name, 0, 0, NI_NAMEREQD))) {
		h_errno = herr(e);
		return 0;
	}
	memcpy(r.addr[0], addr, len);
	r.addrs[0] = (char *)r.addr[0];
	r.h.h_name = r.name;
	r.h.h_aliases = r.aliases;
	r.h.h_addrtype = type;
	r.h.h_length = (int)len;
	r.h.h_addr_list = r.addrs;
	return &r.h;
}
