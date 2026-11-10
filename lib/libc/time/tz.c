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

/* The time-zone database.
 *
 * TZ is, in this order of preference:
 *   - unset or empty: /etc/localtime if it is a TZif file, else UTC;
 *   - ":name" or a name that is not a valid POSIX TZ string: the TZif file
 *     name (absolute) or /usr/share/zoneinfo/name (no "..");
 *   - a POSIX TZ string (XBD 8.3): std offset [dst [offset] [,rule]].
 * TZif files of version 2 and later are read with their 64-bit data and
 * the POSIX TZ string of their footer, which governs times after the last
 * transition.  Leap-second entries are ignored.
 *
 * Abbreviation strings are never freed, so tm_zone pointers stay valid
 * (POSIX only requires that until TZ changes).
 */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "time_impl.h"
#include "lock.h"
#include "syscall.h"

struct rule {             /* a POSIX TZ string */
	long stdoff, dstoff;  /* seconds east of UTC */
	const char *stdname, *dstname;
	int hasdst;
	struct { int kind; int m, w, d; long time; } start, end;  /* kind: 'J', 'D', 'M' */
};

static volatile int lock;
static char *cur_tz;            /* the TZ value the state was built from */
static int initialised;

/* TZif data */
static int ntrans, ntypes;
static long long *trans;
static unsigned char *trans_idx;
static struct ttype { long off; int isdst; const char *abbr; } *types;
static int have_footer;
static struct rule footer;      /* also used for a plain POSIX TZ */
static int use_rule;            /* no TZif: the rule alone */

/* __tzname, __timezone, __daylight (and their POSIX names) are defined
 * in arch/x86_64/tzdata.s. */

void __tz_lock(void) { LOCK(&lock); }
void __tz_unlock(void) { UNLOCK(&lock); }

/* ---- POSIX TZ strings ---- */

static char *save(const char *s, size_t n)
{
	char *p = malloc(n + 1);
	if (!p) return 0;
	memcpy(p, s, n);
	p[n] = 0;
	return p;
}

static int getname(const char **ps, const char **name)
{
	const char *s = *ps, *b;
	size_t n;
	if (*s == '<') {
		b = ++s;
		while ((unsigned)(*s | 32) - 'a' < 26 || (unsigned)*s - '0' < 10 || *s == '+' || *s == '-') s++;
		if (*s != '>') return -1;
		n = (size_t)(s - b);
		s++;
	} else {
		b = s;
		while ((unsigned)(*s | 32) - 'a' < 26) s++;
		n = (size_t)(s - b);
	}
	if (n < 3) return -1;
	if (!(*name = save(b, n))) return -1;
	*ps = s;
	return 0;
}

static int getnum(const char **ps, int max, int *v)
{
	const char *s = *ps;
	int n = 0;
	if ((unsigned)*s - '0' >= 10) return -1;
	while ((unsigned)*s - '0' < 10) { n = n * 10 + (*s++ - '0'); if (n > max) return -1; }
	*v = n;
	*ps = s;
	return 0;
}

/* [+-]hh[:mm[:ss]], hours up to maxh; returns seconds */
static int gettime(const char **ps, int maxh, long *t)
{
	const char *s = *ps;
	int sign = 1, h, m = 0, sec = 0;
	if (*s == '+') s++;
	else if (*s == '-') { sign = -1; s++; }
	if (getnum(&s, maxh, &h)) return -1;
	if (*s == ':') { s++; if (getnum(&s, 59, &m)) return -1;
		if (*s == ':') { s++; if (getnum(&s, 59, &sec)) return -1; } }
	*t = sign * (h * 3600L + m * 60L + sec);
	*ps = s;
	return 0;
}

static int getrdate(const char **ps, int *kind, int *m, int *w, int *d)
{
	const char *s = *ps;
	if (*s == 'J') { s++; *kind = 'J'; if (getnum(&s, 365, d) || *d < 1) return -1; }
	else if (*s == 'M') {
		s++; *kind = 'M';
		if (getnum(&s, 12, m) || *m < 1 || *s++ != '.' || getnum(&s, 5, w) || *w < 1 ||
		    *s++ != '.' || getnum(&s, 6, d)) return -1;
	} else { *kind = 'D'; if (getnum(&s, 365, d)) return -1; }
	*ps = s;
	return 0;
}

static int parse_rule(const char *s, struct rule *r)
{
	long t;
	memset(r, 0, sizeof *r);
	if (getname(&s, &r->stdname) || gettime(&s, 24, &t)) return -1;
	r->stdoff = -t;
	r->dstoff = r->stdoff;
	r->dstname = r->stdname;
	if (!*s) return 0;
	if (getname(&s, &r->dstname)) return -1;
	r->hasdst = 1;
	r->dstoff = r->stdoff + 3600;
	if (*s && *s != ',') {
		if (gettime(&s, 24, &t)) return -1;
		r->dstoff = -t;
	}
	if (!*s) {  /* no rule given: the US rules, as other systems use */
		r->start.kind = r->end.kind = 'M';
		r->start.m = 3; r->start.w = 2; r->start.d = 0; r->start.time = 7200;
		r->end.m = 11; r->end.w = 1; r->end.d = 0; r->end.time = 7200;
		return 0;
	}
	if (*s++ != ',') return -1;
	if (getrdate(&s, &r->start.kind, &r->start.m, &r->start.w, &r->start.d)) return -1;
	r->start.time = 7200;
	if (*s == '/' && (s++, gettime(&s, 167, &r->start.time))) return -1;
	if (*s++ != ',') return -1;
	if (getrdate(&s, &r->end.kind, &r->end.m, &r->end.w, &r->end.d)) return -1;
	r->end.time = 7200;
	if (*s == '/' && (s++, gettime(&s, 167, &r->end.time))) return -1;
	return *s ? -1 : 0;
}

/* local seconds since the Epoch at which a rule date of year y starts */
static long long rule_local(long long y, int kind, int m, int w, int d, long time)
{
	long long jan1 = __days_from_civil(y, 1, 1), days;
	int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
	if (kind == 'J') days = jan1 + d - 1 + (leap && d >= 60);
	else if (kind == 'D') days = jan1 + d;
	else {
		long long first = __days_from_civil(y, (unsigned)m, 1);
		int wd1 = (int)(((first % 7) + 11) % 7);     /* weekday of the 1st */
		int mdays = m == 2 ? 28 + leap : 30 + ((m + (m > 7)) & 1);
		int day = 1 + (d - wd1 + 7) % 7 + 7 * (w - 1);
		if (day > mdays) day -= 7;                     /* w == 5: the last one */
		days = first + day - 1;
	}
	return days * 86400 + time;
}

static int rule_isdst(const struct rule *r, long long t)
{
	struct tm tm;
	long long y, start, end;
	if (!r->hasdst) return 0;
	if (__secs_to_tm(t + r->stdoff, &tm)) return 0;
	y = tm.tm_year + 1900LL;
	start = rule_local(y, r->start.kind, r->start.m, r->start.w, r->start.d, r->start.time) - r->stdoff;
	end = rule_local(y, r->end.kind, r->end.m, r->end.w, r->end.d, r->end.time) - r->dstoff;
	if (start < end) return t >= start && t < end;
	return !(t >= end && t < start);   /* southern hemisphere */
}

/* ---- TZif files ---- */

static long long be(const unsigned char *p, int n)
{
	unsigned long long v = 0;
	int i;
	for (i = 0; i < n; i++) v = v << 8 | p[i];
	if (n == 4) return (long long)(int)(unsigned)v;
	return (long long)v;
}

static unsigned char *readfile(const char *path, size_t *len)
{
	long fd = __syscall4(SYS_openat, -100, path, 02000000 /* O_CLOEXEC */, 0);
	unsigned char *buf = 0, *nb;
	size_t cap = 0, n = 0;
	if (fd < 0) return 0;
	for (;;) {
		long r;
		if (n == cap) {
			cap = cap ? 2 * cap : 4096;
			if (cap > (1 << 22) || !(nb = realloc(buf, cap))) { free(buf); buf = 0; break; }
			buf = nb;
		}
		r = __syscall3(SYS_read, fd, buf + n, cap - n);
		if (r < 0) { free(buf); buf = 0; break; }
		if (!r) break;
		n += (size_t)r;
	}
	__syscall1(SYS_close, fd);
	*len = n;
	return buf;
}

static int load_tzif(const char *path)
{
	size_t len, off;
	unsigned char *d = readfile(path, &len), *p;
	long long cnt[6];
	int i, v2, tsize;
	char *abbrs;

	if (!d) return -1;
	if (len < 44 || memcmp(d, "TZif", 4)) goto bad;
	v2 = d[4] >= '2';
	for (i = 0; i < 6; i++) cnt[i] = be(d + 20 + 4 * i, 4);
	off = 44;
	tsize = 4;
	if (v2) {  /* skip the 32-bit block */
		off += (size_t)(cnt[3] * 5 + cnt[4] * 6 + cnt[5] + cnt[2] * 8 + cnt[1] + cnt[0]);
		if (off + 44 > len || memcmp(d + off, "TZif", 4)) goto bad;
		for (i = 0; i < 6; i++) cnt[i] = be(d + off + 20 + 4 * i, 4);
		off += 44;
		tsize = 8;
	}
	for (i = 0; i < 6; i++) if (cnt[i] < 0 || cnt[i] > 100000) goto bad;
	if (cnt[4] < 1) goto bad;
	if (off + (size_t)(cnt[3] * (tsize + 1) + cnt[4] * 6 + cnt[5] + cnt[2] * (tsize + 4) + cnt[1] + cnt[0]) > len)
		goto bad;

	ntrans = (int)cnt[3];
	ntypes = (int)cnt[4];
	trans = malloc((size_t)ntrans * sizeof *trans + 1);
	trans_idx = malloc((size_t)ntrans + 1);
	types = malloc((size_t)ntypes * sizeof *types);
	abbrs = malloc((size_t)cnt[5] + 1);
	if (!trans || !trans_idx || !types || !abbrs) goto bad;
	p = d + off;
	for (i = 0; i < ntrans; i++, p += tsize) trans[i] = be(p, tsize);
	for (i = 0; i < ntrans; i++, p++) trans_idx[i] = *p < ntypes ? *p : 0;
	{
		unsigned char *tp = p;
		p += ntypes * 6;
		memcpy(abbrs, p, (size_t)cnt[5]);
		abbrs[cnt[5]] = 0;
		for (i = 0; i < ntypes; i++, tp += 6) {
			types[i].off = (long)be(tp, 4);
			types[i].isdst = tp[4] != 0;
			types[i].abbr = tp[5] < cnt[5] ? abbrs + tp[5] : "";
		}
		p += cnt[5];
	}
	p += cnt[2] * (tsize + 4) + cnt[1] + cnt[0];
	have_footer = 0;
	if (v2 && p < d + len && *p == '\n') {
		unsigned char *e = memchr(p + 1, '\n', (size_t)(d + len - p - 1));
		if (e && e > p + 1) {
			char *s = save((char *)p + 1, (size_t)(e - p - 1));
			if (s && !parse_rule(s, &footer)) have_footer = 1;
		}
	}
	free(d);
	return 0;
bad:
	free(d);
	ntrans = ntypes = 0;
	return -1;
}

static void set_utc(void)
{
	ntrans = ntypes = 0;
	use_rule = 1;
	memset(&footer, 0, sizeof footer);
	footer.stdname = footer.dstname = "UTC";
}

void __tz_update(void)
{
	const char *tz = getenv("TZ");
	char path[PATH_MAX];
	size_t l;

	if (!tz) tz = "";
	if (initialised && cur_tz && !strcmp(tz, cur_tz)) return;
	free(cur_tz);
	cur_tz = save(tz, strlen(tz));
	initialised = 1;
	use_rule = 0;
	have_footer = 0;
	ntrans = ntypes = 0;

	if (!*tz) {
		if (load_tzif("/etc/localtime")) set_utc();
	} else if (*tz != ':' && !parse_rule(tz, &footer)) {
		use_rule = 1;
	} else {
		if (*tz == ':') tz++;
		l = strlen(tz);
		if (*tz == '/') {
			if (l >= sizeof path) l = 0;
			else memcpy(path, tz, l + 1);
		} else if (strstr(tz, "..") || l + 21 > sizeof path) {
			l = 0;
		} else {
			memcpy(path, "/usr/share/zoneinfo/", 20);
			memcpy(path + 20, tz, l + 1);
		}
		if (!l || load_tzif(path)) set_utc();
	}

	/* tzname, timezone, daylight */
	if (use_rule) {
		__tzname[0] = (char *)footer.stdname;
		__tzname[1] = (char *)footer.dstname;
		__timezone = -footer.stdoff;
		__daylight = footer.hasdst;
	} else {
		int i, s = -1, dd = -1;
		for (i = ntrans - 1; i >= 0 && (s < 0 || dd < 0); i--) {
			int t = trans_idx[i];
			if (types[t].isdst) { if (dd < 0) dd = t; }
			else if (s < 0) s = t;
		}
		for (i = 0; s < 0 && i < ntypes; i++) if (!types[i].isdst) s = i;
		if (s < 0) s = 0;
		__tzname[0] = (char *)types[s].abbr;
		__tzname[1] = (char *)(dd >= 0 ? types[dd].abbr : types[s].abbr);
		__timezone = -types[s].off;
		__daylight = dd >= 0;
		if (have_footer) {
			__tzname[0] = (char *)footer.stdname;
			__tzname[1] = (char *)footer.dstname;
			__timezone = -footer.stdoff;
			__daylight = footer.hasdst;
		}
	}
}

void __tz_lookup(long long t, long *off, int *isdst, const char **abbr)
{
	int i;
	if (use_rule || (ntrans && t >= trans[ntrans - 1] && have_footer)) {
		int dst = rule_isdst(&footer, t);
		*off = dst ? footer.dstoff : footer.stdoff;
		*isdst = dst;
		*abbr = dst ? footer.dstname : footer.stdname;
		return;
	}
	if (!ntrans || t < trans[0]) {
		for (i = 0; i < ntypes && types[i].isdst; i++);
		if (i == ntypes) i = 0;
	} else {
		int lo = 0, hi = ntrans - 1;   /* last transition <= t */
		while (lo < hi) {
			int mid = lo + (hi - lo + 1) / 2;
			if (trans[mid] <= t) lo = mid; else hi = mid - 1;
		}
		i = trans_idx[lo];
	}
	if (!ntypes) { *off = 0; *isdst = 0; *abbr = "UTC"; return; }
	*off = types[i].off;
	*isdst = types[i].isdst;
	*abbr = types[i].abbr;
}

void __tz_offsets(long long t, long *stdoff, long *dstoff)
{
	long off;
	int dst, k;
	const char *a;
	*stdoff = *dstoff = 0;
	if (use_rule || !ntrans) { *stdoff = footer.stdoff; *dstoff = footer.dstoff; if (!use_rule && ntypes) *stdoff = *dstoff = types[0].off; return; }
	__tz_lookup(t, &off, &dst, &a);
	*stdoff = *dstoff = off;
	/* look around t (a year either way) for the other kind */
	for (k = -2; k <= 2; k++) {
		long o; int d;
		__tz_lookup(t + k * 15778800LL, &o, &d, &a);
		if (d) *dstoff = o; else *stdoff = o;
	}
	if (dst) *dstoff = off; else *stdoff = off;
}
