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


/* route - show and change the routing table (IPv4), through the routing socket.
 *
 *   route [-nqv] show
 *   route [-nqv] get destination
 *   route [-nqv] add [-net|-host] destination[/prefixlen] [gateway] [-netmask mask]
 *   route [-nqv] delete [-net|-host] destination[/prefixlen] [gateway] [-netmask mask]
 *   route [-nqv] flush
 *
 * The destination is an address or "default"; -netmask mask (dotted, 0xhex or
 * /prefixlen) gives a network's mask, a bare address is a host.  The gateway is
 * an address on a network the system is on.  flush removes the routes through
 * gateways.  Names are never looked up (-n is accepted).  The table is the
 * kernel's: sysctl net.route (NET_RT_DUMP) and RTM_ADD, RTM_DELETE and RTM_GET
 * messages on a PF_ROUTE socket. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <unistd.h>

static int sock = -1, quiet, seq;

struct rt {
	uint32_t dst, mask, gw;	/* host byte order */
	int flags;
	unsigned index;
};

static void usage(void)
{
	fprintf(stderr, "usage: route [-nqv] command [[-net | -host] destination[/prefixlen] [gateway] [-netmask mask]]\n"
	    "commands: show, get, add, delete, flush\n");
	exit(1);
}

static void die(const char *what, const char *arg)
{
	fprintf(stderr, "route: %s%s%s\n", what, arg ? ": " : "", arg ? arg : "");
	exit(1);
}

static int parse_ip(const char *s, uint32_t *host)
{
	struct in_addr a;
	if (inet_pton(AF_INET, s, &a) != 1)
		return -1;
	*host = ntohl(a.s_addr);
	return 0;
}

static int parse_mask(const char *s, uint32_t *mask)
{
	char *end;
	unsigned long v;
	if (s[0] == '/') {
		v = strtoul(s + 1, &end, 10);
		if (*end || v > 32)
			return -1;
		*mask = v ? 0xffffffffu << (32 - v) : 0;
		return 0;
	}
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		v = strtoul(s, &end, 16);
		if (*end || v > 0xffffffffUL)
			return -1;
		*mask = (uint32_t)v;
		return 0;
	}
	return parse_ip(s, mask);
}

static int prefixlen(uint32_t mask)
{
	int n = 0;
	while (mask & 0x80000000u) {
		n++;
		mask <<= 1;
	}
	return n;
}

static const char *ip_str(uint32_t host)
{
	static char buf[4][INET_ADDRSTRLEN];
	static int i;
	struct in_addr a;
	i = (i + 1) & 3;
	a.s_addr = htonl(host);
	inet_ntop(AF_INET, &a, buf[i], sizeof buf[i]);
	return buf[i];
}

/* the table, from the kernel */
static int dump(struct rt **out)
{
	int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0};
	size_t len = 0, n = 0, cap = 0;
	char *buf, *p, *end;
	struct rt *t = NULL;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
		die("net.route", strerror(errno));
	len += len / 8 + 1024;
	buf = malloc(len);
	if (!buf || sysctl(mib, 6, buf, &len, NULL, 0) == -1)
		die("net.route", strerror(errno));
	for (p = buf, end = buf + len; p + sizeof(struct rt_msghdr) <= end; ) {
		struct rt_msghdr *m = (struct rt_msghdr *)p;
		char *a = (char *)(m + 1);
		struct rt r;
		int i;
		if (m->rtm_msglen == 0)
			break;
		memset(&r, 0, sizeof r);
		r.flags = m->rtm_flags;
		r.index = m->rtm_index;
		for (i = 0; i < RTAX_MAX; i++) {
			struct sockaddr *sa = (struct sockaddr *)a;
			struct in_addr in;
			if (!(m->rtm_addrs & (1 << i)))
				continue;
			memset(&in, 0, sizeof in);
			if (i == RTAX_NETMASK) {	/* a mask is cut where its zeros end */
				size_t k = sa->sa_len > 4 ? sa->sa_len - 4 : 0;
				if (k > sizeof in)
					k = sizeof in;
				memcpy(&in, (char *)sa + 4, k);
				r.mask = ntohl(in.s_addr);
			} else if (sa->sa_family == AF_INET) {
				memcpy(&in, (char *)sa + 4, sizeof in);
				if (i == RTAX_DST)
					r.dst = ntohl(in.s_addr);
				else if (i == RTAX_GATEWAY)
					r.gw = ntohl(in.s_addr);
			}
			a += SA_SIZE(sa);
		}
		if (!(r.flags & RTF_HOST) && !(m->rtm_addrs & RTA_NETMASK))
			r.mask = 0;
		if (n == cap) {
			cap = cap ? cap * 2 : 16;
			t = realloc(t, cap * sizeof *t);
			if (!t)
				die("out of memory", NULL);
		}
		t[n++] = r;
		p += m->rtm_msglen;
	}
	free(buf);
	*out = t;
	return (int)n;
}

static void flags_str(int f, char *out)
{
	*out++ = 'U';
	if (f & RTF_GATEWAY) *out++ = 'G';
	if (f & RTF_HOST) *out++ = 'H';
	if (f & RTF_REJECT) *out++ = 'R';
	if (f & RTF_STATIC) *out++ = 'S';
	if (f & RTF_BLACKHOLE) *out++ = 'B';
	if (f & RTF_DYNAMIC) *out++ = 'D';
	*out = 0;
}

static void dest_str(const struct rt *r, char *out, size_t size)
{
	if (r->dst == 0 && r->mask == 0 && !(r->flags & RTF_HOST))
		snprintf(out, size, "default");
	else if (r->flags & RTF_HOST)
		snprintf(out, size, "%s", ip_str(r->dst));
	else
		snprintf(out, size, "%s/%d", ip_str(r->dst), prefixlen(r->mask));
}

static void show(void)
{
	struct rt *t;
	int i, n = dump(&t);
	printf("%-20s %-16s %-6s %s\n", "Destination", "Gateway", "Flags", "Netif");
	for (i = 0; i < n; i++) {
		char dst[64], fl[16], ifn[IF_NAMESIZE] = "";
		dest_str(&t[i], dst, sizeof dst);
		flags_str(t[i].flags, fl);
		if (t[i].index)
			if_indextoname(t[i].index, ifn);
		printf("%-20s %-16s %-6s %s\n", dst, t[i].gw ? ip_str(t[i].gw) : "link", fl, ifn);
	}
	free(t);
}

/* one message: RTM_* type with the addresses dst, gateway (gw != 0), netmask (mask given) */
static int rtsend(int type, uint32_t dst, int havemask, uint32_t mask, int havegw, uint32_t gw, int flags)
{
	struct {
		struct rt_msghdr h;
		struct sockaddr_in a[3];
	} m;
	int n = 0;

	memset(&m, 0, sizeof m);
	m.h.rtm_version = RTM_VERSION;
	m.h.rtm_type = (unsigned char)type;
	m.h.rtm_flags = flags | RTF_UP;
	m.h.rtm_pid = getpid();
	m.h.rtm_seq = ++seq;
	m.h.rtm_addrs = RTA_DST;
	m.a[n].sin_len = sizeof m.a[n]; m.a[n].sin_family = AF_INET; m.a[n].sin_addr.s_addr = htonl(dst); n++;
	if (havegw) {
		m.h.rtm_addrs |= RTA_GATEWAY;
		m.a[n].sin_len = sizeof m.a[n]; m.a[n].sin_family = AF_INET; m.a[n].sin_addr.s_addr = htonl(gw); n++;
	}
	if (havemask) {
		m.h.rtm_addrs |= RTA_NETMASK;
		m.a[n].sin_len = sizeof m.a[n]; m.a[n].sin_family = AF_INET; m.a[n].sin_addr.s_addr = htonl(mask); n++;
	}
	m.h.rtm_msglen = (unsigned short)(sizeof m.h + n * sizeof m.a[0]);
	return (int)write(sock, &m, m.h.rtm_msglen) == m.h.rtm_msglen ? 0 : -1;
}

static void say(const char *verb, const char *kind, const char *dest, const char *gw, const char *err)
{
	if (err)
		fprintf(stderr, "route: %s %s %s%s%s: %s\n", verb, kind, dest, gw ? " " : "", gw ? gw : "", err);
	else if (!quiet)
		printf("%s %s %s%s%s\n", verb, kind, dest, gw ? ": gateway " : "", gw ? gw : "");
}

static void get(const char *dest)
{
	uint32_t host;
	struct rt *t;
	int i, n, best = -1;
	if (parse_ip(dest, &host) == -1)
		die("bad address", dest);
	n = dump(&t);
	for (i = 0; i < n; i++)	/* the longest mask that covers it */
		if ((host & t[i].mask) == (t[i].dst & t[i].mask) && (best < 0 || t[i].mask >= t[best].mask))
			best = i;
	if (best < 0)
		die("not in table", dest);
	{
		char ifn[IF_NAMESIZE] = "", d[64], fl[16];
		dest_str(&t[best], d, sizeof d);
		if (t[best].index)
			if_indextoname(t[best].index, ifn);
		flags_str(t[best].flags, fl);
		printf("   route to: %s\ndestination: %s\n       mask: %s\n", dest, ip_str(t[best].dst), ip_str(t[best].mask));
		if (t[best].gw)
			printf("    gateway: %s\n", ip_str(t[best].gw));
		printf("  interface: %s\n      flags: %s\n", ifn, fl);
	}
	free(t);
}

static void change(int add, char **av)
{
	const char *dest = NULL, *gwarg = NULL, *kind = "net";
	uint32_t dst = 0, mask = 0, gw = 0;
	int ishost = -1, havemask = 0, i;
	char *slash, buf[64];

	for (i = 0; av[i]; i++) {
		if (!strcmp(av[i], "-net")) ishost = 0;
		else if (!strcmp(av[i], "-host")) ishost = 1;
		else if (!strcmp(av[i], "-netmask")) {
			if (!av[++i] || parse_mask(av[i], &mask) == -1)
				die("bad netmask", av[i]);
			havemask = 1;
		} else if (av[i][0] == '-') {
			die("option not supported", av[i]);
		} else if (!dest) dest = av[i];
		else if (!gwarg) gwarg = av[i];
		else usage();
	}
	if (!dest)
		usage();
	snprintf(buf, sizeof buf, "%s", dest);
	if ((slash = strchr(buf, '/')) != NULL) {
		*slash = 0;
		if (parse_mask(dest + (slash - buf), &mask) == -1)
			die("bad prefix length", dest);
		havemask = 1;
	}
	if (!strcmp(buf, "default")) {
		dst = 0; mask = 0; havemask = 1;
	} else if (parse_ip(buf, &dst) == -1) {
		die("bad address", dest);
	}
	if (gwarg && parse_ip(gwarg, &gw) == -1)
		die("bad gateway", gwarg);
	if (ishost == -1)
		ishost = !havemask;
	if (ishost)
		kind = "host";
	if (add && !gwarg)
		die("a gateway is needed", dest);
	if (rtsend(add ? RTM_ADD : RTM_DELETE, dst, !ishost && havemask, mask, gwarg != NULL, gw,
	    (gwarg ? RTF_GATEWAY : 0) | (ishost ? RTF_HOST : 0) | RTF_STATIC) == -1) {
		say(add ? "add" : "delete", kind, dest, gwarg, strerror(errno));
		exit(1);
	}
	say(add ? "add" : "delete", kind, dest, gwarg, NULL);
}

static void flush(void)
{
	struct rt *t;
	int i, n = dump(&t);
	for (i = 0; i < n; i++) {
		if (!(t[i].flags & RTF_GATEWAY))
			continue;
		if (rtsend(RTM_DELETE, t[i].dst, !(t[i].flags & RTF_HOST), t[i].mask, 1, t[i].gw, t[i].flags) == -1) {
			if (errno != ESRCH)
				fprintf(stderr, "route: delete %s: %s\n", ip_str(t[i].dst), strerror(errno));
		} else if (!quiet) {
			printf("%s\n", ip_str(t[i].dst));
		}
	}
	free(t);
}

int main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "nqv")) != -1) {
		switch (ch) {
		case 'n': case 'v': break;
		case 'q': quiet = 1; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();
	if (!strcmp(argv[0], "show") || !strcmp(argv[0], "get") || !strcmp(argv[0], "flush") ||
	    !strcmp(argv[0], "add") || !strcmp(argv[0], "delete")) {
		sock = socket(PF_ROUTE, SOCK_RAW, AF_INET);
		if (sock == -1)
			die("socket", strerror(errno));
	}
	if (!strcmp(argv[0], "show") && argc == 1)
		show();
	else if (!strcmp(argv[0], "get") && argc == 2)
		get(argv[1]);
	else if (!strcmp(argv[0], "add"))
		change(1, argv + 1);
	else if (!strcmp(argv[0], "delete"))
		change(0, argv + 1);
	else if (!strcmp(argv[0], "flush") && argc == 1)
		flush();
	else
		usage();
	return 0;
}
