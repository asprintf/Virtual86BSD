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

/* sysctl - get and set kernel state (the sysctl(8) of OpenBSD, with fewer
   nodes: those the kernel answers, see sys/kern/kern_sysctl.c).
 *
 *   sysctl [-Aanq] [-f file] [name[=value] ...]
 *
 * A name is a path of words, kern.ostype, as <sys/sysctl.h> lists them.
 * `name' prints name=value (-n: the value alone); `name=value' sets it (root;
 * the old value and the new one are printed as `name: old -> new', -q: not).
 * A name that is a node (kern) prints everything below it; no name, -a and -A
 * print everything the kernel answers.  -f reads `name=value' lines (# starts
 * a comment).  The exit status is 1 if a name given on the command line or in
 * the file could not be read or set (OpenBSD's is always 0). */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

struct list {
	const struct ctlname *l;
	int n;
};
#define LIST(a) { a, (int)(sizeof a / sizeof a[0]) }

static const struct ctlname top_names[] = CTL_NAMES, kern_names[] = CTL_KERN_NAMES, vm_names[] = CTL_VM_NAMES,
    hw_names[] = CTL_HW_NAMES, net_names[] = CTL_NET_NAMES, ipproto_names[] = CTL_IPPROTO_NAMES,
    ip_names[] = IPCTL_NAMES, icmp_names[] = ICMPCTL_NAMES, tcp_names[] = TCPCTL_NAMES;
static const struct list top = LIST(top_names), kern = LIST(kern_names), vm = LIST(vm_names), hw = LIST(hw_names),
    net = LIST(net_names), ipproto = LIST(ipproto_names), ip = LIST(ip_names), icmp = LIST(icmp_names),
    tcp = LIST(tcp_names);

static int aflag, nflag, qflag, status;

/* the names below the node mib[0..depth) is (NULL: the program knows none) */
static const struct list *children(const int *mib, int depth)
{
	if (depth == 1) {
		switch (mib[0]) {
		case CTL_KERN: return &kern;
		case CTL_VM: return &vm;
		case CTL_HW: return &hw;
		case CTL_NET: return &net;
		}
	} else if (depth == 2 && mib[0] == CTL_NET && mib[1] == PF_INET) {
		return &ipproto;
	} else if (depth == 3 && mib[0] == CTL_NET && mib[1] == PF_INET && mib[2] == IPPROTO_IP) {
		return &ip;
	} else if (depth == 3 && mib[0] == CTL_NET && mib[1] == PF_INET && mib[2] == IPPROTO_ICMP) {
		return &icmp;
	} else if (depth == 3 && mib[0] == CTL_NET && mib[1] == PF_INET && mib[2] == IPPROTO_TCP) {
		return &tcp;
	}
	return NULL;
}

/* a name that is there (a gap has type 0) */
static int present(const struct list *l, int i)
{
	return l->l[i].ctl_name != NULL && l->l[i].ctl_type != 0;
}

static void warnmsg(const char *what, const char *why)
{
	fprintf(stderr, "sysctl: %s: %s\n", what, why);
	status = 1;
}

/* print the value of the node mib[0..depth) of the given type; set it first
   if newval is not NULL.  explicit: it was asked for by name (errors are
   reported); not: part of a listing, where what the kernel does not answer
   is left out. */
static void show(const char *name, const int *mib, int depth, int type, const char *newval, int explicit)
{
	char buf[4096], *s;
	size_t size = sizeof buf - 1;
	int iv, niv = 0;
	long long qv, nqv = 0;
	char *end;
	void *newp = NULL;
	size_t newlen = 0;

	if (newval) {
		switch (type) {
		case CTLTYPE_INT:
			errno = 0;
			qv = strtoll(newval, &end, 0);
			if (*newval == 0 || *end || errno || qv < INT_MIN || qv > INT_MAX) {
				warnmsg(name, "value is invalid");
				return;
			}
			niv = (int)qv;
			newp = &niv;
			newlen = sizeof niv;
			break;
		case CTLTYPE_QUAD:
			errno = 0;
			qv = strtoll(newval, &end, 0);
			if (*newval == 0 || *end || errno) {
				warnmsg(name, "value is invalid");
				return;
			}
			nqv = qv;
			newp = &nqv;
			newlen = sizeof nqv;
			break;
		case CTLTYPE_STRING:
			newp = (void *)newval;
			newlen = strlen(newval);
			break;
		default:
			warnmsg(name, "a structure cannot be set");
			return;
		}
	}
	if (sysctl(mib, (unsigned)depth, buf, &size, newp, newlen) == -1) {
		if (!explicit)
			return;
		switch (errno) {
		case EOPNOTSUPP: warnmsg(name, "value is not available"); break;
		case ENOTDIR: warnmsg(name, "specification is incomplete"); break;
		case EPERM: warnmsg(name, "Operation not permitted"); break;
		case ENOSYS: warnmsg(name, "Function not implemented (not a V86BSD program?)"); break;
		default: warnmsg(name, strerror(errno));
		}
		return;
	}
	buf[size < sizeof buf ? size : sizeof buf - 1] = 0;
	s = buf;
	if (mib[0] == CTL_KERN && mib[1] == KERN_CLOCKRATE && size == sizeof(struct clockinfo)) {
		struct clockinfo ci;
		memcpy(&ci, buf, sizeof ci);
		if (!nflag) printf("%s=", name);
		printf("tick = %d, hz = %d, profhz = %d, stathz = %d\n", ci.tick, ci.hz, ci.profhz, ci.stathz);
	} else if (mib[0] == CTL_KERN && mib[1] == KERN_BOOTTIME && size >= 2 * sizeof(long)) {
		long bt[2];
		time_t t;
		memcpy(bt, buf, sizeof bt);
		t = (time_t)bt[0];
		if (nflag) printf("%lld\n", (long long)t);
		else printf("%s=%s", name, ctime(&t));
	} else if (mib[0] == CTL_VM && mib[1] == VM_LOADAVG && size == sizeof(struct loadavg)) {
		struct loadavg la;
		memcpy(&la, buf, sizeof la);
		if (!nflag) printf("%s=", name);
		printf("%.2f %.2f %.2f\n", (double)la.ldavg[0] / (double)la.fscale, (double)la.ldavg[1] / (double)la.fscale,
		    (double)la.ldavg[2] / (double)la.fscale);
	} else if (type == CTLTYPE_INT && size == sizeof(int)) {
		memcpy(&iv, buf, sizeof iv);
		if (newval) { if (!qflag) { if (nflag) printf("%d\n", niv); else printf("%s: %d -> %d\n", name, iv, niv); } }
		else { if (!nflag) printf("%s=", name); printf("%d\n", iv); }
	} else if (type == CTLTYPE_QUAD && size == sizeof(long long)) {
		memcpy(&qv, buf, sizeof qv);
		if (newval) { if (!qflag) { if (nflag) printf("%lld\n", nqv); else printf("%s: %lld -> %lld\n", name, qv, nqv); } }
		else { if (!nflag) printf("%s=", name); printf("%lld\n", qv); }
	} else if (type == CTLTYPE_STRING) {
		if (newval) { if (!qflag) { if (nflag) printf("%s\n", newval); else printf("%s: %s -> %s\n", name, s, newval); } }
		else { if (!nflag) printf("%s=", name); printf("%s\n", s); }
	} else if (explicit) {
		warnmsg(name, "type is unknown to this program");
	}
}

/* every leaf below the node `prefix' (mib[0..depth)) that the kernel answers */
static void listall(const char *prefix, int *mib, int depth, const struct list *l)
{
	int i;
	char name[256];

	for (i = 0; i < l->n; i++) {
		if (!present(l, i))
			continue;
		mib[depth] = i;
		snprintf(name, sizeof name, "%s%s%s", prefix, depth ? "." : "", l->l[i].ctl_name);
		if (l->l[i].ctl_type == CTLTYPE_NODE) {
			const struct list *c = children(mib, depth + 1);
			if (c)
				listall(name, mib, depth + 1, c);
		} else {
			show(name, mib, depth + 1, l->l[i].ctl_type, NULL, 0);
		}
	}
}

/* one argument: name or name=value */
static void parse(const char *arg)
{
	char buf[512], full[512], *val, *w, *next;
	int mib[CTL_MAXNAME], depth = 0, i, found;
	const struct list *l = &top;

	if (strlen(arg) >= sizeof buf) {
		warnmsg(arg, "name too long");
		return;
	}
	strcpy(buf, arg);
	val = strchr(buf, '=');
	if (val) {
		*val++ = 0;
		while (*val == ' ' || *val == '\t')
			val++;
	}
	strcpy(full, buf); /* the name alone, for what is printed */
	for (w = buf; w; w = next) {
		next = strchr(w, '.');
		if (next)
			*next++ = 0;
		if (depth >= CTL_MAXNAME) {
			warnmsg(full, "name too long");
			return;
		}
		found = -1;
		for (i = 0; i < l->n; i++)
			if (present(l, i) && strcmp(l->l[i].ctl_name, w) == 0) {
				found = i;
				break;
			}
		if (found < 0) {
			fprintf(stderr, "sysctl: level name %s in %s is invalid\n", w, full);
			status = 1;
			return;
		}
		mib[depth++] = found;
		if (l->l[found].ctl_type == CTLTYPE_NODE) {
			const struct list *c = children(mib, depth);
			if (!next) { /* a node: everything below */
				if (val) {
					warnmsg(full, "a node cannot be set");
				} else if (c) {
					char pre[512];
					snprintf(pre, sizeof pre, "%s", full);
					listall(pre, mib, depth, c);
				} else {
					warnmsg(full, "class is not implemented");
				}
				return;
			}
			if (!c) {
				warnmsg(full, "class is not implemented");
				return;
			}
			l = c;
		} else {
			if (next) {
				fprintf(stderr, "sysctl: name %s in %s is unknown\n", next, full);
				status = 1;
				return;
			}
			show(full, mib, depth, l->l[found].ctl_type, val, 1);
			return;
		}
	}
}

static void readfile(const char *file)
{
	FILE *fp = fopen(file, "r");
	char line[512], *p, *e;

	if (!fp) {
		fprintf(stderr, "sysctl: %s: %s\n", file, strerror(errno));
		status = 1;
		return;
	}
	while (fgets(line, sizeof line, fp)) {
		if ((e = strchr(line, '#')))
			*e = 0;
		for (p = line; *p == ' ' || *p == '\t'; p++)
			;
		e = p + strlen(p);
		while (e > p && (e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
			*--e = 0;
		if (*p)
			parse(p);
	}
	fclose(fp);
}

static void usage(void)
{
	fprintf(stderr, "usage: sysctl [-Aanq] [-f file] [name[=value] ...]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch, mib[CTL_MAXNAME];
	const char *conffile = NULL;

	while ((ch = getopt(argc, argv, "Aaf:nqw")) != -1) {
		switch (ch) {
		case 'A':
		case 'a': aflag = 1; break;
		case 'f': conffile = optarg; break;
		case 'n': nflag = 1; break;
		case 'q': qflag = 1; break;
		case 'w': break; /* name=value sets anyway */
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((argc == 0 && !conffile) || aflag) {
		int i;
		for (i = 1; i < top.n; i++) {
			const struct list *c;
			if (!present(&top, i))
				continue;
			mib[0] = i;
			c = children(mib, 1);
			if (c)
				listall(top.l[i].ctl_name, mib, 1, c);
		}
		return 0;
	}
	if (conffile)
		readfile(conffile);
	for (; *argv; argv++)
		parse(*argv);
	return status;
}
