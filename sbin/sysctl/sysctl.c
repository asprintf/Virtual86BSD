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


/* sysctl - get and set kernel state through FreeBSD's self-describing MIB.
 *
 *   sysctl [-Aanq] [-f file] [name[=value] ...]
 *
 * A name is a path of words, kern.ostype.  `name' prints name=value (-n: the
 * value alone); `name=value' sets it (root; `name: old -> new', -q: not).  A
 * name that is a node (kern) prints everything below it; no name, -a and -A
 * print everything the kernel answers.  -f reads `name=value' lines (# starts a
 * comment).  The exit status is 1 if a name could not be read or set.
 *
 * The kernel's own tables say what a name is: sysctl.name2oid (name to
 * number), sysctl.next (the next name), sysctl.name and sysctl.oidfmt (the
 * type and the format); see sys/kern/kern_sysctl.c. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MAXOID (CTL_MAXNAME + 2)

static int nflag, qflag, status;

static void warnmsg(const char *what, const char *why)
{
	fprintf(stderr, "sysctl: %s: %s\n", what, why);
	status = 1;
}

/* the kernel's answer to the meta call {0, which, oid...} */
static int meta(int which, const int *oid, size_t len, void *buf, size_t *size)
{
	int q[MAXOID];
	q[0] = CTL_SYSCTL;
	q[1] = which;
	memcpy(q + 2, oid, len * sizeof(int));
	return sysctl(q, (unsigned)len + 2, buf, size, NULL, 0);
}

static int oidname(const int *oid, size_t len, char *name, size_t size)
{
	size_t n = size;
	if (meta(CTL_SYSCTL_NAME, oid, len, name, &n) < 0)
		return -1;
	return 0;
}

/* the kind (CTLTYPE_* and flags) and the format string of a name */
static int oidfmt(const int *oid, size_t len, unsigned *kind, char *fmt, size_t fmtsize)
{
	unsigned char buf[256];
	size_t n = sizeof buf;
	if (meta(4, oid, len, buf, &n) < 0 || n < sizeof(unsigned))
		return -1;
	memcpy(kind, buf, sizeof *kind);
	snprintf(fmt, fmtsize, "%s", (char *)buf + sizeof(unsigned));
	return 0;
}

static int nextoid(const int *oid, size_t len, int *next, size_t *nextlen)
{
	size_t n = CTL_MAXNAME * sizeof(int);
	if (meta(CTL_SYSCTL_NEXT, oid, len, next, &n) < 0)
		return -1;
	*nextlen = n / sizeof(int);
	return 0;
}

static int nametooid(const char *name, int *oid, size_t *len)
{
	int q[2] = {CTL_SYSCTL, CTL_SYSCTL_NAME2OID};
	size_t n = CTL_MAXNAME * sizeof(int);
	if (sysctl(q, 2, oid, &n, name, strlen(name)) < 0)
		return -1;
	*len = n / sizeof(int);
	return 0;
}

/* one number of a kind at p, printed */
static void printnum(FILE *f, unsigned type, const void *p, const char *fmt)
{
	switch (type) {
	case CTLTYPE_INT: fprintf(f, fmt[1] == 'U' ? "%u" : "%d", *(const int *)p); break;
	case CTLTYPE_UINT: fprintf(f, "%u", *(const unsigned *)p); break;
	case CTLTYPE_LONG: fprintf(f, "%ld", *(const long *)p); break;
	case CTLTYPE_ULONG: fprintf(f, "%lu", *(const unsigned long *)p); break;
	case CTLTYPE_S64: fprintf(f, "%lld", (long long)*(const int64_t *)p); break;
	case CTLTYPE_U64: fprintf(f, "%llu", (unsigned long long)*(const uint64_t *)p); break;
	case CTLTYPE_U8: fprintf(f, "%u", *(const uint8_t *)p); break;
	case CTLTYPE_U16: fprintf(f, "%u", *(const uint16_t *)p); break;
	case CTLTYPE_S8: fprintf(f, "%d", *(const int8_t *)p); break;
	case CTLTYPE_S16: fprintf(f, "%d", *(const int16_t *)p); break;
	case CTLTYPE_S32: fprintf(f, "%d", *(const int32_t *)p); break;
	case CTLTYPE_U32: fprintf(f, "%u", *(const uint32_t *)p); break;
	}
}

static size_t numsize(unsigned type)
{
	switch (type) {
	case CTLTYPE_INT: case CTLTYPE_UINT: case CTLTYPE_S32: case CTLTYPE_U32: return 4;
	case CTLTYPE_LONG: case CTLTYPE_ULONG: case CTLTYPE_S64: case CTLTYPE_U64: return 8;
	case CTLTYPE_U8: case CTLTYPE_S8: return 1;
	case CTLTYPE_U16: case CTLTYPE_S16: return 2;
	}
	return 0;
}

/* a value of an opaque kind that the program knows; 0: it does not */
static int printopaque(const char *fmt, const unsigned char *v, size_t n)
{
	if (!strcmp(fmt, "S,timeval") && n >= sizeof(struct timeval)) {
		struct timeval tv;
		time_t t;
		char date[64];
		memcpy(&tv, v, sizeof tv);
		t = tv.tv_sec;
		strftime(date, sizeof date, "%a %b %e %H:%M:%S %Y", localtime(&t));
		printf("{ sec = %lld, usec = %lld } %s", (long long)tv.tv_sec, (long long)tv.tv_usec, date);
		return 1;
	}
	if (!strcmp(fmt, "S,loadavg") && n >= 3 * sizeof(uint32_t) + sizeof(long)) {
		uint32_t a[3];
		long scale;
		memcpy(a, v, sizeof a);
		memcpy(&scale, v + 3 * sizeof(uint32_t) + 4, sizeof scale);
		if (scale <= 0)
			return 0;
		printf("{ %.2f %.2f %.2f }", (double)a[0] / scale, (double)a[1] / scale, (double)a[2] / scale);
		return 1;
	}
	return 0;
}

/* the value of oid, with its name unless -n; set it first if newval is not NULL.
   explicit: asked for by name (errors are reported); not: part of a listing,
   where what the kernel does not answer is left out. */
static void show(const char *name, const int *oid, size_t len, const char *newval, int explicit)
{
	unsigned kind, type;
	char fmt[64];
	static unsigned char buf[65536];
	unsigned char nbuf[4096];
	size_t size = sizeof buf - 1, i, w;
	int prefix = !nflag;

	if (oidfmt(oid, len, &kind, fmt, sizeof fmt) < 0) {
		if (explicit)
			warnmsg(name, strerror(errno));
		return;
	}
	type = kind & 0xf;
	if (type == CTLTYPE_NODE)
		return;
	if (sysctl(oid, (unsigned)len, buf, &size, NULL, 0) < 0) {
		if (explicit)
			warnmsg(name, strerror(errno));
		return;
	}
	if (type == CTLTYPE_OPAQUE && !explicit && strncmp(fmt, "S,", 2) != 0 && fmt[0] != 'A')
		return;
	if (newval) {
		size_t nl = 0;
		void *np = nbuf;
		char *end;
		long long q;
		if (!(kind & CTLFLAG_WR)) {
			warnmsg(name, "oid is read-only");
			return;
		}
		if (type == CTLTYPE_STRING) {
			np = (void *)newval;
			nl = strlen(newval) + 1;
		} else if ((w = numsize(type)) != 0) {
			errno = 0;
			if (type == CTLTYPE_ULONG || type == CTLTYPE_U64 || type == CTLTYPE_UINT || type == CTLTYPE_U32 ||
			    type == CTLTYPE_U16 || type == CTLTYPE_U8)
				q = (long long)strtoull(newval, &end, 0);
			else
				q = strtoll(newval, &end, 0);
			if (*newval == 0 || *end || errno) {
				warnmsg(name, "value is invalid");
				return;
			}
			memcpy(nbuf, &q, w);	/* little-endian */
			nl = w;
		} else {
			warnmsg(name, "cannot set a value of this type");
			return;
		}
		if (!qflag) {
			printf("%s: ", name);
			buf[size] = 0;
			if (type == CTLTYPE_STRING)
				printf("%s", (char *)buf);
			else
				printnum(stdout, type, buf, fmt);
			printf(" -> ");
		}
		if (sysctl(oid, (unsigned)len, NULL, NULL, np, nl) < 0) {
			if (!qflag)
				printf("\n");
			warnmsg(name, strerror(errno));
			return;
		}
		if (!qflag)
			printf("%s\n", newval);
		return;
	}
	if (prefix)
		printf("%s=", name);
	if (type == CTLTYPE_STRING) {
		buf[size] = 0;
		printf("%s\n", (char *)buf);
	} else if ((w = numsize(type)) != 0) {
		for (i = 0; i + w <= size; i += w) {
			if (i)
				putchar(' ');
			printnum(stdout, type, buf + i, fmt);
		}
		putchar('\n');
	} else {
		if (!printopaque(fmt, buf, size)) {
			printf("Format:%s Length:%zu Dump:0x", fmt, size);
			for (i = 0; i < size && i < 64; i++)
				printf("%02x", buf[i]);
			if (size > 64)
				printf("...");
		}
		putchar('\n');
	}
}

/* everything below the node oid[0..len) (all of it for len 0) */
static void walk(const int *start, size_t len)
{
	int oid[CTL_MAXNAME], nx[CTL_MAXNAME];
	size_t olen, nlen;
	char name[512];

	if (len) {
		memcpy(oid, start, len * sizeof(int));
		olen = len;
	} else {
		oid[0] = CTL_KERN;
		olen = 1;
		if (oidname(oid, olen, name, sizeof name) == 0)
			show(name, oid, olen, NULL, 0);
	}
	for (;;) {
		if (nextoid(oid, olen, nx, &nlen) < 0 || nlen == 0)
			break;
		if (len && (nlen <= len || memcmp(nx, start, len * sizeof(int)) != 0))
			break;
		memcpy(oid, nx, nlen * sizeof(int));
		olen = nlen;
		if (oidname(oid, olen, name, sizeof name) == 0)
			show(name, oid, olen, NULL, 0);
	}
}

static void one(const char *arg)
{
	char *name = strdup(arg), *eq;
	const char *val = NULL;
	int oid[CTL_MAXNAME];
	size_t len;
	unsigned kind;
	char fmt[64];

	if (!name)
		return;
	if ((eq = strchr(name, '=')) != NULL) {
		*eq = 0;
		val = eq + 1;
	}
	if (nametooid(name, oid, &len) < 0) {
		fprintf(stderr, "sysctl: unknown oid '%s'\n", name);
		status = 1;
	} else if (oidfmt(oid, len, &kind, fmt, sizeof fmt) == 0 && (kind & 0xf) == CTLTYPE_NODE) {
		if (val)
			warnmsg(name, "cannot set a node");
		else
			walk(oid, len);
	} else {
		show(name, oid, len, val, 1);
	}
	free(name);
}

static void usage(void)
{
	fprintf(stderr, "usage: sysctl [-Aanqw] [-f file] [name[=value] ...]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch, all = 0;
	const char *file = NULL;

	while ((ch = getopt(argc, argv, "Aanqwf:")) != -1) {
		switch (ch) {
		case 'A': case 'a': all = 1; break;
		case 'n': nflag = 1; break;
		case 'q': qflag = 1; break;
		case 'w': break;	/* FreeBSD's: name=value sets, with or without it */
		case 'f': file = optarg; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (file) {
		FILE *f = strcmp(file, "-") ? fopen(file, "r") : stdin;
		char line[1024], *p;
		if (!f) {
			warnmsg(file, strerror(errno));
			return 1;
		}
		while (fgets(line, sizeof line, f)) {
			if ((p = strchr(line, '#')) != NULL)
				*p = 0;
			p = line + strspn(line, " \t");
			p[strcspn(p, "\n")] = 0;
			for (ch = (int)strlen(p); ch > 0 && (p[ch - 1] == ' ' || p[ch - 1] == '\t'); )
				p[--ch] = 0;
			if (*p)
				one(p);
		}
		if (f != stdin)
			fclose(f);
	}
	if (argc == 0 && !file)
		all = 1;
	if (all && argc == 0)
		walk(NULL, 0);
	for (; argc > 0; argc--, argv++)
		one(*argv);
	return status;
}
