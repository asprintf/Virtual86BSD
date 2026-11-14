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

/* XTI options (t_optmgmt) on sockets: the XTI, TCP, UDP and IP levels */
#define _DEFAULT_SOURCE
#include <xti.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "xti_impl.h"

enum { K_UINT, K_BOOL, K_RO, K_UCHAR, K_LINGER, K_KEEPALIVE, K_CSUM, K_NONE };

struct od {
	unsigned level, name;
	int kind, sol, sopt;
};

static const struct od defs[] = {
	{ XTI_GENERIC, XTI_DEBUG, K_UINT, SOL_SOCKET, SO_DEBUG },
	{ XTI_GENERIC, XTI_LINGER, K_LINGER, SOL_SOCKET, SO_LINGER },
	{ XTI_GENERIC, XTI_RCVBUF, K_UINT, SOL_SOCKET, SO_RCVBUF },
	{ XTI_GENERIC, XTI_RCVLOWAT, K_UINT, SOL_SOCKET, SO_RCVLOWAT },
	{ XTI_GENERIC, XTI_SNDBUF, K_UINT, SOL_SOCKET, SO_SNDBUF },
	{ XTI_GENERIC, XTI_SNDLOWAT, K_UINT, SOL_SOCKET, SO_SNDLOWAT },
	{ T_INET_TCP, T_TCP_NODELAY, K_BOOL, IPPROTO_TCP, TCP_NODELAY },
	{ T_INET_TCP, T_TCP_MAXSEG, K_RO, IPPROTO_TCP, TCP_MAXSEG },
	{ T_INET_TCP, T_TCP_KEEPALIVE, K_KEEPALIVE, SOL_SOCKET, SO_KEEPALIVE },
	{ T_INET_UDP, T_UDP_CHECKSUM, K_CSUM, 0, 0 },
	{ T_INET_IP, T_IP_OPTIONS, K_NONE, 0, 0 },
	{ T_INET_IP, T_IP_TOS, K_UCHAR, IPPROTO_IP, IP_TOS },
	{ T_INET_IP, T_IP_TTL, K_UCHAR, IPPROTO_IP, IP_TTL },
	{ T_INET_IP, T_IP_REUSEADDR, K_BOOL, SOL_SOCKET, SO_REUSEADDR },
	{ T_INET_IP, T_IP_DONTROUTE, K_BOOL, SOL_SOCKET, SO_DONTROUTE },
	{ T_INET_IP, T_IP_BROADCAST, K_BOOL, SOL_SOCKET, SO_BROADCAST },
};
#define NDEFS (sizeof defs / sizeof *defs)

static unsigned size_of(const struct od *d)
{
	switch (d->kind) {
	case K_UCHAR: return 1;
	case K_LINGER: return sizeof(struct t_linger);
	case K_KEEPALIVE: return sizeof(struct t_kpalive);
	case K_NONE: return 0;
	default: return sizeof(t_uscalar_t);
	}
}

static int level_known(unsigned level)
{
	unsigned i;
	for (i = 0; i < NDEFS; i++)
		if (defs[i].level == level) return 1;
	return 0;
}

static const struct od *find(unsigned level, unsigned name)
{
	unsigned i;
	for (i = 0; i < NDEFS; i++)
		if (defs[i].level == level && defs[i].name == name) return &defs[i];
	return 0;
}

/* the value of option D of socket FD, in VAL (size_of(D) bytes); 0 or -1 */
static int get(int fd, const struct od *d, void *val)
{
	int v;
	socklen_t l = sizeof v;
	switch (d->kind) {
	case K_CSUM:
		*(t_uscalar_t *)val = T_YES;
		return 0;
	case K_NONE:
		return -1;
	case K_LINGER: {
		struct linger g;
		struct t_linger *t = val;
		l = sizeof g;
		if (getsockopt(fd, d->sol, d->sopt, &g, &l) < 0) return -1;
		t->l_onoff = g.l_onoff ? T_YES : T_NO;
		t->l_linger = g.l_linger;
		return 0;
	}
	case K_KEEPALIVE: {
		struct t_kpalive *k = val;
		if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, &l) < 0) return -1;
		k->kp_onoff = v ? T_YES : T_NO;
		l = sizeof v;
		if (getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &v, &l) < 0) v = 7200;
		k->kp_timeout = (v + 59) / 60;
		return 0;
	}
	}
	if (getsockopt(fd, d->sol, d->sopt, &v, &l) < 0) return -1;
	if (d->kind == K_UCHAR) *(unsigned char *)val = (unsigned char)v;
	else if (d->kind == K_BOOL) *(t_uscalar_t *)val = v ? T_YES : T_NO;
	else *(t_uscalar_t *)val = (t_uscalar_t)v;
	return 0;
}

/* set option D of socket FD to VAL; 0 or -1 */
static int set(int fd, const struct od *d, const void *val)
{
	int v;
	switch (d->kind) {
	case K_LINGER: {
		const struct t_linger *t = val;
		struct linger g;
		g.l_onoff = t->l_onoff != T_NO;
		g.l_linger = t->l_linger;
		return setsockopt(fd, d->sol, d->sopt, &g, sizeof g);
	}
	case K_KEEPALIVE: {
		const struct t_kpalive *k = val;
		v = k->kp_onoff != T_NO;
		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof v) < 0) return -1;
		if (v && k->kp_timeout > 0 && k->kp_timeout != (t_scalar_t)T_UNSPEC) {
			v = k->kp_timeout * 60;
			setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &v, sizeof v);
		}
		return 0;
	}
	case K_UCHAR:
		v = *(const unsigned char *)val;
		break;
	case K_BOOL:
		v = *(const t_uscalar_t *)val != T_NO;
		break;
	default:
		v = (int)*(const t_uscalar_t *)val;
		break;
	}
	return setsockopt(fd, d->sol, d->sopt, &v, sizeof v);
}

static int readonly(const struct od *d)
{
	return d->kind == K_RO || d->kind == K_CSUM;
}

/* the default value of D: that of a new socket of TYPE */
static int getdefault(int type, const struct od *d, void *val)
{
	int s = socket(AF_INET, type, 0), r;
	if (s < 0) return -1;
	r = get(s, d, val);
	close(s);
	return r;
}

static int is_unspec(const struct od *d, const void *val)
{
	switch (d->kind) {
	case K_UCHAR: return 0;
	case K_LINGER: return ((const struct t_linger *)val)->l_onoff == (t_scalar_t)T_UNSPEC;
	case K_KEEPALIVE: return ((const struct t_kpalive *)val)->kp_onoff == (t_scalar_t)T_UNSPEC;
	default: return *(const t_uscalar_t *)val == (t_uscalar_t)T_UNSPEC;
	}
}

/* the status of one option for ACTION; the value to answer with goes in VAL */
static int one(int fd, int type, int action, const struct od *d, const void *in, unsigned inlen, void *val)
{
	unsigned sz = size_of(d);
	unsigned char tmp[16];
	int s, st;

	if (d->kind == K_NONE) return T_NOTSUPPORT;
	memset(val, 0, sizeof tmp);
	switch (action) {
	case T_CURRENT:
		if (get(fd, d, val) < 0) return T_NOTSUPPORT;
		return readonly(d) ? T_READONLY : T_SUCCESS;
	case T_DEFAULT:
		if (getdefault(type, d, val) < 0) return T_NOTSUPPORT;
		return readonly(d) ? T_READONLY : T_SUCCESS;
	}
	/* T_NEGOTIATE and T_CHECK */
	if (inlen && inlen != sz) return -1;
	if (readonly(d)) {
		get(fd, d, val);
		return T_READONLY;
	}
	if (!inlen) {
		if (action == T_CHECK) return T_SUCCESS;
		memset(tmp, 0, sizeof tmp);
		if (d->name == XTI_DEBUG && d->level == XTI_GENERIC) {
			set(fd, d, tmp);
			return T_SUCCESS;
		}
		if (getdefault(type, d, tmp) < 0 || set(fd, d, tmp) < 0) return T_FAILURE;
		get(fd, d, val);
		return T_SUCCESS;
	}
	memcpy(tmp, in, sz);
	if (is_unspec(d, tmp) && getdefault(type, d, tmp) < 0) return T_FAILURE;
	if (action == T_CHECK) {
		s = socket(AF_INET, type, 0);
		if (s < 0) return T_FAILURE;
		st = set(s, d, tmp);
		close(s);
		memcpy(val, in, sz);
		if (st < 0) return errno == EPERM || errno == EACCES ? T_NOTSUPPORT : T_FAILURE;
		return T_SUCCESS;
	}
	if (set(fd, d, tmp) < 0) {
		memcpy(val, in, sz);
		return errno == EPERM || errno == EACCES ? T_NOTSUPPORT : T_FAILURE;
	}
	if (get(fd, d, val) < 0 || memcmp(val, tmp, sz) != 0) {
		/* the sizes and the low-water marks are not absolute requirements */
		if (d->kind == K_UINT && d->name != XTI_DEBUG) return T_PARTSUCCESS;
		memcpy(val, in, sz);
	}
	return T_SUCCESS;
}

static int worse(int a, int b)
{
	static const int order[] = { T_SUCCESS, T_PARTSUCCESS, T_FAILURE, T_READONLY, T_NOTSUPPORT };
	unsigned i, ra = 0, rb = 0;
	for (i = 0; i < 5; i++) {
		if (order[i] == a) ra = i;
		if (order[i] == b) rb = i;
	}
	return rb > ra ? b : a;
}

static int fail(int e)
{
	t_errno = e;
	return -1;
}

int __xti_opts(int fd, int type, int action, const void *in, unsigned inlen,
    void *out, unsigned outmax, unsigned *outlen)
{
	const unsigned char *p = in;
	unsigned char *o = out, val[16];
	unsigned off = 0, n = 0, level = 0, i, sz;
	int first = 1, overall = T_SUCCESS, st;
	struct t_opthdr h, r;

	if (action != T_NEGOTIATE && action != T_CHECK && action != T_DEFAULT && action != T_CURRENT)
		return fail(TBADFLAG);
	while (off < inlen) {
		if (inlen - off < sizeof h) return fail(TBADOPT);
		memcpy(&h, p + off, sizeof h);
		if (h.len < sizeof h || h.len > inlen - off) return fail(TBADOPT);
		if (first) level = h.level;
		first = 0;
		if (h.level != level || !level_known(h.level)) return fail(TBADOPT);
		if (h.name == T_ALLOPT) {
			if (action == T_CHECK) return fail(TBADOPT);
			for (i = 0; i < NDEFS; i++) {
				if (defs[i].level != level || defs[i].kind == K_NONE) continue;
				sz = size_of(&defs[i]);
				st = one(fd, type, action == T_NEGOTIATE ? T_DEFAULT : action, &defs[i], 0, 0, val);
				if (action == T_NEGOTIATE && st == T_SUCCESS) {
					set(fd, &defs[i], val);
					get(fd, &defs[i], val);
				}
				if (n + T_ALIGN(sizeof r + sz) > outmax) return fail(TBUFOVFLW);
				r.len = sizeof r + sz; r.level = level; r.name = defs[i].name; r.status = st;
				memcpy(o + n, &r, sizeof r);
				memcpy(o + n + sizeof r, val, sz);
				n += T_ALIGN(r.len);
				overall = worse(overall, st);
			}
			break;
		}
		{
			const struct od *d = find(h.level, h.name);
			unsigned vl = h.len - sizeof h;
			if (!d) {
				sz = vl;
				st = T_NOTSUPPORT;
				memcpy(val, p + off + sizeof h, sz > sizeof val ? sizeof val : sz);
			} else {
				sz = size_of(d);
				st = one(fd, type, action, d, p + off + sizeof h, vl, val);
				if (st < 0) return fail(TBADOPT);
			}
			if (n + T_ALIGN(sizeof r + sz) > outmax) return fail(TBUFOVFLW);
			r.len = sizeof r + sz; r.level = h.level; r.name = h.name; r.status = st;
			memcpy(o + n, &r, sizeof r);
			memcpy(o + n + sizeof r, val, sz > sizeof val ? sizeof val : sz);
			n += T_ALIGN(r.len);
			overall = worse(overall, st);
		}
		off += T_ALIGN(h.len);
	}
	*outlen = n;
	return overall;
}

void __xti_copyopts(int from, int to)
{
	unsigned i;
	unsigned char v[16];
	for (i = 0; i < NDEFS; i++) {
		if (readonly(&defs[i]) || defs[i].kind == K_NONE) continue;
		if (get(from, &defs[i], v) == 0) set(to, &defs[i], v);
	}
}
