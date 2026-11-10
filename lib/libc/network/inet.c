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

/* <arpa/inet.h>: address conversions.  inet_pton is strict (dotted
 * decimal, no leading zeros, as glibc); inet_addr takes the old forms
 * a, a.b, a.b.c, a.b.c.d with C-style numbers; inet_ntop writes IPv6
 * addresses as RFC 5952 (and glibc) do. */
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* a, a.b, a.b.c, a.b.c.d: 1, or 0 */
static int aton(const char *s, struct in_addr *res)
{
	unsigned long v[4];
	int n = 0;
	for (;;) {
		unsigned long x = 0;
		int base = 10, digits = 0;
		if (*s == '0') {
			base = 8;
			if (s[1] == 'x' || s[1] == 'X') {
				base = 16;
				s += 2;
			}
		}
		for (;; s++) {
			int d;
			if (*s >= '0' && *s <= '9') d = *s - '0';
			else if (base == 16 && *s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
			else if (base == 16 && *s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
			else break;
			if (d >= base)
				return 0;
			x = x * base + d;
			if (x > 0xffffffffUL)
				return 0;
			digits++;
		}
		if (!digits && base != 8)
			return 0;
		v[n++] = x;
		if (*s == '.' && n < 4) {
			s++;
			continue;
		}
		break;
	}
	if (*s)
		return 0;
	switch (n) {
	case 1: break;
	case 2: if (v[0] > 255 || v[1] > 0xffffff) return 0; v[0] = v[0] << 24 | v[1]; break;
	case 3: if (v[0] > 255 || v[1] > 255 || v[2] > 0xffff) return 0; v[0] = v[0] << 24 | v[1] << 16 | v[2]; break;
	case 4: if (v[0] > 255 || v[1] > 255 || v[2] > 255 || v[3] > 255) return 0;
		v[0] = v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]; break;
	}
	res->s_addr = htonl((uint32_t)v[0]);
	return 1;
}

in_addr_t inet_addr(const char *s)
{
	struct in_addr a;
	return aton(s, &a) ? a.s_addr : INADDR_NONE;
}

int __inet_aton(const char *s, struct in_addr *a)
{
	return aton(s, a);
}

char *inet_ntoa(struct in_addr a)
{
	static char buf[16];
	unsigned char *p = (unsigned char *)&a;
	snprintf(buf, sizeof buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return buf;
}

static int pton4(const char *s, unsigned char *a)
{
	int i, j, v;
	for (i = 0; i < 4; i++) {
		if (*s < '0' || *s > '9')
			return 0;
		for (v = 0, j = 0; *s >= '0' && *s <= '9'; s++, j++) {
			if (j && !v)
				return 0;           /* a leading zero */
			v = v * 10 + *s - '0';
			if (v > 255)
				return 0;
		}
		a[i] = (unsigned char)v;
		if (i < 3 && *s++ != '.')
			return 0;
	}
	return !*s;
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int pton6(const char *s, unsigned char *a)
{
	unsigned short w[8];
	int n = 0, gap = -1, i, d, v, k;
	if (*s == ':' && *++s != ':')
		return 0;
	for (;;) {
		if (*s == ':') {
			if (gap >= 0)
				return 0;
			gap = n;
			if (!*++s)
				break;
			continue;
		}
		/* an embedded IPv4 address ends the address */
		if (n <= 6 && strchr(s, '.') && !strchr(s, ':')) {
			unsigned char v4[4];
			if (!pton4(s, v4))
				return 0;
			w[n++] = (unsigned short)(v4[0] << 8 | v4[1]);
			w[n++] = (unsigned short)(v4[2] << 8 | v4[3]);
			break;
		}
		for (v = 0, k = 0; k < 4 && (d = hexval(*s)) >= 0; k++, s++)
			v = v * 16 + d;
		if (!k || n == 8)
			return 0;
		w[n++] = (unsigned short)v;
		if (!*s)
			break;
		if (*s != ':' || n == 8)
			return 0;
		if (!*++s)
			return 0;          /* a trailing single ':' */
	}
	if (gap < 0 ? n != 8 : n == 8)
		return 0;
	if (gap >= 0) {
		int move = n - gap;
		memmove(w + 8 - move, w + gap, move * sizeof *w);
		for (i = gap; i < 8 - move; i++)
			w[i] = 0;
	}
	for (i = 0; i < 8; i++) {
		a[2 * i] = (unsigned char)(w[i] >> 8);
		a[2 * i + 1] = (unsigned char)w[i];
	}
	return 1;
}

int inet_pton(int af, const char *__RESTRICT s, void *__RESTRICT a)
{
	if (af == AF_INET)
		return pton4(s, a);
	if (af == AF_INET6)
		return pton6(s, a);
	errno = EAFNOSUPPORT;
	return -1;
}

const char *inet_ntop(int af, const void *__RESTRICT src, char *__RESTRICT dst, socklen_t size)
{
	const unsigned char *a = src;
	char buf[64], *p = buf;
	if (af == AF_INET) {
		snprintf(buf, sizeof buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	} else if (af == AF_INET6) {
		unsigned w[8];
		int i, best = -1, bestlen = 0, cur = -1, len = 0;
		for (i = 0; i < 8; i++)
			w[i] = (unsigned)a[2 * i] << 8 | a[2 * i + 1];
		for (i = 0; i < 8; i++) {
			if (!w[i]) {
				if (cur < 0) { cur = i; len = 0; }
				if (++len > bestlen) { best = cur; bestlen = len; }
			} else
				cur = -1;
		}
		if (bestlen < 2)
			best = -1;
		for (i = 0; i < 8; i++) {
			if (i == best) {
				*p++ = ':';
				if (i == 0)
					*p++ = ':';
				i += bestlen - 1;
				continue;
			}
			/* an IPv4 tail: ::a.b.c.d and ::ffff:a.b.c.d */
			if (i == 6 && best == 0 && (bestlen == 6 || (bestlen == 5 && w[5] == 0xffff))) {
				p += sprintf(p, "%d.%d.%d.%d", a[12], a[13], a[14], a[15]);
				break;
			}
			p += sprintf(p, "%x", w[i]);
			if (i < 7)
				*p++ = ':';
		}
		*p = 0;
	} else {
		errno = EAFNOSUPPORT;
		return 0;
	}
	if (strlen(buf) >= size) {
		errno = ENOSPC;
		return 0;
	}
	return strcpy(dst, buf);
}
