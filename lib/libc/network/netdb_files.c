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

/* The files of the network databases, as getaddrinfo and getnameinfo
 * use them: /etc/hosts and /etc/services. */
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "netdb_impl.h"

const char *__hosts_path = "/etc/hosts";
const char *__services_path = "/etc/services";

int __netdb_fields(char *line, char **f, int max)
{
	int n = 0;
	char *p = strchr(line, '#');
	if (p)
		*p = 0;
	for (p = line; n < max;) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (!*p)
			break;
		f[n++] = p;
		while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
			p++;
		if (*p)
			*p++ = 0;
	}
	return n;
}

/* a numeric address with an optional %scope (IPv6): 1, or 0 */
static int parse_addr(struct __address *a, const char *s)
{
	char buf[64], *pct;
	memset(a, 0, sizeof *a);
	if (inet_pton(AF_INET, s, a->addr) == 1) {
		a->family = AF_INET;
		return 1;
	}
	if (strlen(s) >= sizeof buf)
		return 0;
	strcpy(buf, s);
	if ((pct = strchr(buf, '%')))
		*pct = 0;
	if (inet_pton(AF_INET6, buf, a->addr) != 1)
		return 0;
	a->family = AF_INET6;
	return 1;
}

int __hosts_lookup(struct __address *buf, int max, char canon[256], const char *name, int family)
{
	FILE *f = fopen(__hosts_path, "re");
	char line[1024], *fl[64];
	int n = 0, nf, i;
	if (!f)
		return -1;
	while (n < max && fgets(line, sizeof line, f)) {
		struct __address a;
		nf = __netdb_fields(line, fl, 64);
		if (nf < 2 || !parse_addr(&a, fl[0]))
			continue;
		if (family != AF_UNSPEC && a.family != family)
			continue;
		for (i = 1; i < nf; i++)
			if (!strcasecmp(fl[i], name))
				break;
		if (i == nf)
			continue;
		if (!n && strlen(fl[1]) < 256)
			strcpy(canon, fl[1]);
		buf[n++] = a;
	}
	fclose(f);
	return n;
}

int __hosts_reverse(const unsigned char *addr, int family, char *name, size_t n)
{
	FILE *f = fopen(__hosts_path, "re");
	char line[1024], *fl[3];
	int found = 0;
	if (!f)
		return 0;
	while (!found && fgets(line, sizeof line, f)) {
		struct __address a;
		if (__netdb_fields(line, fl, 3) < 2 || !parse_addr(&a, fl[0]) || a.family != family)
			continue;
		if (memcmp(a.addr, addr, family == AF_INET ? 4 : 16))
			continue;
		if (strlen(fl[1]) < n) {
			strcpy(name, fl[1]);
			found = 1;
		}
	}
	fclose(f);
	return found;
}

int __services_lookup(const char *name, const char *proto)
{
	FILE *f = fopen(__services_path, "re");
	char line[1024], *fl[64], *slash;
	int nf, i, port = -1;
	if (!f)
		return -1;
	while (port < 0 && fgets(line, sizeof line, f)) {
		nf = __netdb_fields(line, fl, 64);
		if (nf < 2 || !(slash = strchr(fl[1], '/')) || strcmp(slash + 1, proto))
			continue;
		for (i = 0; i < nf; i++)
			if (i != 1 && !strcmp(fl[i], name))
				break;
		if (i < nf)
			port = atoi(fl[1]);
	}
	fclose(f);
	return port;
}
