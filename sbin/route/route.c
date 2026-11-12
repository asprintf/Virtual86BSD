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

/* route - show and change the routing table.
 *
 *   route [-nqv] show
 *   route [-nqv] get destination
 *   route [-nqv] add [-net|-host] destination[/prefixlen] [gateway] [options]
 *   route [-nqv] delete [-net|-host] destination[/prefixlen] [options]
 *   route [-nqv] flush
 *
 * The destination is an address or "default"; -netmask mask (dotted, 0xhex
 * or /prefixlen) gives a network's mask, a bare address is a host.  The
 * gateway is an address that is on a network the system is on; -iface says
 * the "gateway" is an interface (its name or one of its addresses) and the
 * route is on that link; -ifp name names the interface, -priority n sets
 * the metric (the lowest wins between routes to the same place).  flush
 * removes the routes through gateways.  Names are never looked up (-n is
 * accepted); the table is the kernel's (sysctl net.route, SIOCADDRT and
 * SIOCDELRT).
 */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <unistd.h>

static int sock, quiet;

static void usage(void)
{
	fprintf(stderr, "usage: route [-nqv] command [[-net | -host] destination [gateway] [-netmask mask]\n"
	    "             [-iface] [-ifp interface] [-priority n]]\n"
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
		return 0;
	*host = ntohl(a.s_addr);
	return 1;
}

static int parse_mask(const char *s, uint32_t *mask)
{
	char *end;
	unsigned long v;

	if (s[0] == '/') {
		v = strtoul(s + 1, &end, 10);
		if (!s[1] || *end || v > 32)
			return 0;
		*mask = v == 0 ? 0 : ~0U << (32 - v);
		return 1;
	}
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		v = strtoul(s, &end, 16);
		if (*end || v > 0xffffffffUL)
			return 0;
		*mask = (uint32_t)v;
		return 1;
	}
	return parse_ip(s, mask);
}

static int prefixlen(uint32_t mask)
{
	int n = 0;

	while (mask & 0x80000000U)
		n++, mask <<= 1;
	return n;
}

static void put_in(struct sockaddr *sa, uint32_t host)
{
	struct sockaddr_in *s = (struct sockaddr_in *)sa;

	memset(sa, 0, sizeof *sa);
	s->sin_family = AF_INET;
	s->sin_addr.s_addr = htonl(host);
}

static const char *ip_str(uint32_t host)
{
	struct in_addr a;

	a.s_addr = htonl(host);
	return inet_ntoa(a);
}

/* ---- the table ---- */

static struct rtinfo *dump(int *n)
{
	int mib[6] = {CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0};
	size_t len;
	struct rtinfo *buf;
	int tries;

	for (tries = 0; tries < 5; tries++) {
		if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
			die("net.route", strerror(errno));
		if (!(buf = malloc(len + sizeof *buf)))
			die("malloc", strerror(errno));
		len += sizeof *buf;
		if (sysctl(mib, 6, buf, &len, NULL, 0) == 0) {
			*n = (int)(len / sizeof *buf);
			return buf;
		}
		free(buf);
		if (errno != ENOMEM)
			die("net.route", strerror(errno));
	}
	die("net.route", "the table keeps growing");
	return NULL;
}

static uint32_t ri_dst(const struct rtinfo *r) { return ntohl(r->ri_dst); }
static uint32_t ri_mask(const struct rtinfo *r) { return ntohl(r->ri_mask); }
static uint32_t ri_gw(const struct rtinfo *r) { return ntohl(r->ri_gateway); }

/* "default", a host's address, or a network as 10.0.2/24 (the octets the
   prefix needs) */
static void dest_str(const struct rtinfo *r, char *out, size_t size)
{
	uint32_t d = ri_dst(r), m = ri_mask(r);
	int p = prefixlen(m), oct, k;
	char tmp[32];

	if (m == 0) {
		snprintf(out, size, "default");
		return;
	}
	if (m == 0xffffffffU) {
		snprintf(out, size, "%s", ip_str(d));
		return;
	}
	oct = (p + 7) / 8;
	tmp[0] = 0;
	for (k = 0; k < oct; k++)
		snprintf(tmp + strlen(tmp), sizeof tmp - strlen(tmp), "%s%u", k ? "." : "", (d >> (24 - 8 * k)) & 0xff);
	snprintf(out, size, "%s/%d", tmp, p);
}

static void show(void)
{
	int n, i;
	struct rtinfo *t = dump(&n);

	printf("Routing tables\n\nInternet:\n");
	printf("%-18s %-18s %-5s %6s  %s\n", "Destination", "Gateway", "Flags", "Metric", "Iface");
	for (i = 0; i < n; i++) {
		char d[48], g[32], fl[8], name[IF_NAMESIZE];
		int k = 0;

		dest_str(&t[i], d, sizeof d);
		if (ri_gw(&t[i]))
			snprintf(g, sizeof g, "%s", ip_str(ri_gw(&t[i])));
		else
			snprintf(g, sizeof g, "link#%d", t[i].ri_index);
		if (t[i].ri_flags & RTF_UP)
			fl[k++] = 'U';
		if (t[i].ri_flags & RTF_GATEWAY)
			fl[k++] = 'G';
		if (t[i].ri_flags & RTF_HOST)
			fl[k++] = 'H';
		fl[k] = 0;
		if (!if_indextoname((unsigned)t[i].ri_index, name))
			strcpy(name, "?");
		printf("%-18s %-18s %-5s %6d  %s\n", d, g, fl, t[i].ri_metric, name);
	}
	free(t);
}

/* the route the kernel would use for host: the longest prefix, then the
   lowest metric, through an interface that is up */
static const struct rtinfo *lookup(const struct rtinfo *t, int n, uint32_t host)
{
	const struct rtinfo *best = NULL;
	int i;

	for (i = 0; i < n; i++) {
		struct ifreq r;
		char name[IF_NAMESIZE];

		if ((host & ri_mask(&t[i])) != ri_dst(&t[i]))
			continue;
		if (!if_indextoname((unsigned)t[i].ri_index, name))
			continue;
		memset(&r, 0, sizeof r);
		strcpy(r.ifr_name, name);
		if (ioctl(sock, SIOCGIFFLAGS, &r) == -1 || !((unsigned short)r.ifr_flags & IFF_UP))
			continue;
		if (!best || ri_mask(&t[i]) > ri_mask(best) ||
		    (ri_mask(&t[i]) == ri_mask(best) && t[i].ri_metric < best->ri_metric))
			best = &t[i];
	}
	return best;
}

static void get(const char *dest)
{
	uint32_t host = 0;
	int n;
	struct rtinfo *t;
	const struct rtinfo *r;
	char name[IF_NAMESIZE];

	if (strcmp(dest, "default") && !parse_ip(dest, &host))
		die("invalid destination", dest);
	t = dump(&n);
	if (!(r = lookup(t, n, host))) {
		fprintf(stderr, "route: get host %s: %s\n", dest, strerror(ENETUNREACH));
		exit(1);
	}
	if (!if_indextoname((unsigned)r->ri_index, name))
		strcpy(name, "?");
	printf("   route to: %s\n", dest);
	printf("destination: %s\n", ri_mask(r) ? ip_str(ri_dst(r)) : "default");
	printf("       mask: %s\n", ri_mask(r) ? ip_str(ri_mask(r)) : "default");
	if (ri_gw(r))
		printf("    gateway: %s\n", ip_str(ri_gw(r)));
	printf("  interface: %s\n", name);
	printf("      flags: <UP%s%s>\n", (r->ri_flags & RTF_GATEWAY) ? ",GATEWAY" : "", (r->ri_flags & RTF_HOST) ? ",HOST" : "");
	free(t);
}

/* ---- add, delete, flush ---- */

static void say(const char *verb, const char *kind, const char *dest, const char *gw, const char *err)
{
	if (err) {
		fprintf(stderr, "route: %s %s %s%s%s: %s\n", verb, kind, dest, gw ? ": gateway " : "", gw ? gw : "", err);
		return;
	}
	if (!quiet)
		printf("%s %s %s%s%s\n", verb, kind, dest, gw ? ": gateway " : "", gw ? gw : "");
}

/* the interface that has the address or is called so */
static const char *iface_of(const char *arg, char *out)
{
	struct ifaddrs *list, *a;
	uint32_t ip;
	const char *r = NULL;

	if (if_nametoindex(arg)) {
		strcpy(out, arg);
		return out;
	}
	if (!parse_ip(arg, &ip) || getifaddrs(&list) == -1)
		return NULL;
	for (a = list; a && !r; a = a->ifa_next)
		if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET &&
		    ntohl(((struct sockaddr_in *)a->ifa_addr)->sin_addr.s_addr) == ip) {
			strcpy(out, a->ifa_name);
			r = out;
		}
	freeifaddrs(list);
	return r;
}

static void change(int add, char **av)
{
	const char *pos[2] = {NULL, NULL}, *ifp = NULL;
	int npos = 0, net = 0, host = 0, iface = 0, prio = -1, have_mask = 0;
	uint32_t dst = 0, mask = 0, gw = 0;
	struct rtentry r;
	char devbuf[IF_NAMESIZE], dtxt[48];
	const char *kind, *gwtxt = NULL;
	char *slash;

	for (; *av; av++) {
		const char *p = *av;

		if (!strcmp(p, "-net"))
			net = 1;
		else if (!strcmp(p, "-host"))
			host = 1;
		else if (!strcmp(p, "-iface") || !strcmp(p, "-interface"))
			iface = 1;
		else if (!strcmp(p, "-netmask") || !strcmp(p, "-ifp") || !strcmp(p, "-priority")) {
			if (!av[1])
				die("needs a value", p);
			av++;
			if (p[1] == 'n') {
				if (!parse_mask(*av, &mask))
					die("invalid netmask", *av);
				have_mask = 1;
			} else if (p[1] == 'i')
				ifp = *av;
			else {
				char *end;
				long v = strtol(*av, &end, 10);

				if (!**av || *end || v < 0 || v > 30000)
					die("invalid priority", *av);
				prio = (int)v;
			}
		} else if (p[0] == '-' && p[1])
			die("unknown option", p);
		else if (npos < 2)
			pos[npos++] = p;
		else
			usage();
	}
	if (npos == 0 || (add && npos == 1 && !ifp) || net + host > 1 || (host && have_mask))
		usage();
	if (!strcmp(pos[0], "default")) {
		dst = 0, mask = 0, have_mask = 1, net = 1;
		kind = "net";
	} else {
		char *tmp = strdup(pos[0]);

		if (!tmp)
			die("malloc", strerror(errno));
		if ((slash = strchr(tmp, '/')) != NULL) {
			if (host || have_mask || !parse_mask(pos[0] + (slash - tmp), &mask))
				die("invalid destination", pos[0]);
			*slash = 0;
			have_mask = 1;
		}
		if (!parse_ip(tmp, &dst))
			die("invalid destination", pos[0]);
		free(tmp);
		if (!net && !have_mask)
			host = 1; /* (a bare address) */
		if (host)
			mask = 0xffffffffU, have_mask = 1;
		kind = host ? "host" : "net";
	}
	memset(&r, 0, sizeof r);
	put_in(&r.rt_dst, dst);
	if (have_mask)
		put_in(&r.rt_genmask, mask);
	r.rt_flags = RTF_UP | (host ? RTF_HOST : 0);
	if (prio >= 0)
		r.rt_metric = (short)(prio + 1);
	if (add) {
		if (npos == 2) {
			gwtxt = pos[1];
			if (iface) {
				if (!iface_of(pos[1], devbuf))
					die("no such interface or address", pos[1]);
				ifp = devbuf;
			} else {
				if (!parse_ip(pos[1], &gw))
					die("invalid gateway", pos[1]);
				put_in(&r.rt_gateway, gw);
				r.rt_flags |= RTF_GATEWAY;
			}
		}
		if (ifp)
			r.rt_dev = (char *)ifp;
	}
	if (ifp && !add) /* (nothing to match: the destination is the key) */
		r.rt_dev = NULL;
	snprintf(dtxt, sizeof dtxt, "%s", !strcmp(pos[0], "default") ? "default" : pos[0]);
	if (ioctl(sock, add ? SIOCADDRT : SIOCDELRT, &r) == -1) {
		say(add ? "add" : "delete", kind, dtxt, gwtxt, strerror(errno));
		exit(1);
	}
	say(add ? "add" : "delete", kind, dtxt, gwtxt, NULL);
}

static void flush(void)
{
	int n, i;
	struct rtinfo *t = dump(&n);

	for (i = 0; i < n; i++) {
		struct rtentry r;
		char d[48];

		if (!(t[i].ri_flags & RTF_GATEWAY))
			continue;
		memset(&r, 0, sizeof r);
		put_in(&r.rt_dst, ri_dst(&t[i]));
		put_in(&r.rt_genmask, ri_mask(&t[i]));
		r.rt_flags = (unsigned short)t[i].ri_flags;
		dest_str(&t[i], d, sizeof d);
		if (ioctl(sock, SIOCDELRT, &r) == -1 && errno != ESRCH) {
			say("delete", ri_mask(&t[i]) == 0xffffffffU ? "host" : "net", d, ip_str(ri_gw(&t[i])), strerror(errno));
			exit(1);
		}
		say("delete", ri_mask(&t[i]) == 0xffffffffU ? "host" : "net", d, ip_str(ri_gw(&t[i])), NULL);
	}
	free(t);
}

int main(int argc, char *argv[])
{
	int c;
	const char *cmd;

	while ((c = getopt(argc, argv, "nqv")) != -1) {
		switch (c) {
		case 'n':
		case 'v':
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();
	cmd = argv[0];
	if ((sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) == -1)
		die("socket", strerror(errno));
	if (!strcmp(cmd, "show") || !strcmp(cmd, "list")) {
		if (argc != 1)
			usage();
		show();
	} else if (!strcmp(cmd, "get")) {
		if (argc != 2)
			usage();
		get(argv[1]);
	} else if (!strcmp(cmd, "add")) {
		change(1, argv + 1);
	} else if (!strcmp(cmd, "delete") || !strcmp(cmd, "del")) {
		change(0, argv + 1);
	} else if (!strcmp(cmd, "flush")) {
		if (argc != 1)
			usage();
		flush();
	} else
		usage();
	return fflush(stdout) == 0 ? 0 : 1;
}
