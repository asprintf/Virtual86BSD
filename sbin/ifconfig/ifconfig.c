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

/* ifconfig - show and set the network interfaces: flags, mtu, metric and
 * the IPv4 addresses (the interface ioctls of <sys/sockio.h>, and
 * getifaddrs(3) to list them).
 *
 *   ifconfig [-a] [-l] [interface [inet] [parameters]]
 *
 * No arguments or -a shows every interface, -l only their names, an
 * interface alone just that one.  Parameters (BSD's names):
 *   address[/prefixlen]  make it the interface's address (the first one: it
 *                        keeps its mask; a new one gets its class' mask)
 *   alias address        add an address (-alias address, delete [address]:
 *                        remove one; without an address the first)
 *   netmask mask         a dotted mask, 0xhex, or /prefixlen
 *   broadcast address    (only the address | ~mask is accepted)
 *   mtu n, metric n, up, down, debug, -debug, link0-2, -link0-2
 */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int sock;

static void usage(void)
{
	fprintf(stderr, "usage: ifconfig [-a] [-l] [interface [inet] [parameters]]\n");
	exit(1);
}

static void ioc(unsigned long cmd, const char *cname, void *arg)
{
	if (ioctl(sock, cmd, arg) == -1) {
		fprintf(stderr, "ifconfig: %s: %s\n", cname, strerror(errno));
		exit(1);
	}
}
#define IOC(cmd, arg) ioc(cmd, #cmd, arg)

static void setname(struct ifreq *r, const char *name)
{
	memset(r, 0, sizeof *r);
	if (strlen(name) >= IFNAMSIZ) {
		fprintf(stderr, "ifconfig: interface name too long: %s\n", name);
		exit(1);
	}
	strcpy(r->ifr_name, name);
}

static void put_in(struct sockaddr *sa, uint32_t host)
{
	struct sockaddr_in *s = (struct sockaddr_in *)sa;

	memset(sa, 0, sizeof *sa);
	s->sin_family = AF_INET;
	s->sin_addr.s_addr = htonl(host);
}

static uint32_t get_in(const struct sockaddr *sa)
{
	return ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
}

/* ---- showing ---- */

static const char *const flagnames[16] = {"UP", "BROADCAST", "DEBUG", "LOOPBACK", "POINTOPOINT", "STATICARP",
    "RUNNING", "NOARP", "PROMISC", "ALLMULTI", "OACTIVE", "SIMPLEX", "LINK0", "LINK1", "LINK2", "MULTICAST"};

static void show(const struct ifaddrs *list, const struct ifaddrs *link)
{
	struct ifreq r;
	const struct ifaddrs *a;
	const struct sockaddr_dl *dl = (const struct sockaddr_dl *)link->ifa_addr;
	unsigned f = link->ifa_flags;
	const char *sep = "";
	int i;

	printf("%s: flags=%x<", link->ifa_name, f);
	for (i = 0; i < 16; i++)
		if (f & (1U << i)) {
			printf("%s%s", sep, flagnames[i]);
			sep = ",";
		}
	printf(">");
	setname(&r, link->ifa_name);
	if (ioctl(sock, SIOCGIFMETRIC, &r) == 0 && r.ifr_metric)
		printf(" metric %d", r.ifr_metric);
	setname(&r, link->ifa_name);
	if (ioctl(sock, SIOCGIFMTU, &r) == 0)
		printf(" mtu %d", r.ifr_mtu);
	printf("\n");
	if (dl->sdl_alen == 6) {
		const unsigned char *m = (const unsigned char *)LLADDR(dl);

		printf("\tlladdr %02x:%02x:%02x:%02x:%02x:%02x\n", m[0], m[1], m[2], m[3], m[4], m[5]);
	}
	printf("\tindex %d\n", dl->sdl_index);
	for (a = list; a; a = a->ifa_next) {
		if (strcmp(a->ifa_name, link->ifa_name) || !a->ifa_addr || a->ifa_addr->sa_family != AF_INET)
			continue;
		printf("\tinet %s", inet_ntoa(((struct sockaddr_in *)a->ifa_addr)->sin_addr));
		if (a->ifa_netmask)
			printf(" netmask 0x%08x", get_in(a->ifa_netmask));
		if (a->ifa_dstaddr && (a->ifa_flags & IFF_BROADCAST))
			printf(" broadcast %s", inet_ntoa(((struct sockaddr_in *)a->ifa_dstaddr)->sin_addr));
		printf("\n");
	}
}

/* ---- setting ---- */

static int parse_ip(const char *s, uint32_t *host)
{
	struct in_addr a;

	if (inet_pton(AF_INET, s, &a) != 1)
		return 0;
	*host = ntohl(a.s_addr);
	return 1;
}

static uint32_t prefix_mask(unsigned long n)
{
	return n == 0 ? 0 : ~0U << (32 - n);
}

/* a mask: dotted, 0xhex or /prefixlen */
static int parse_mask(const char *s, uint32_t *mask)
{
	char *end;
	unsigned long v;

	if (s[0] == '/' || (s[0] >= '0' && s[0] <= '9' && !strchr(s, '.') && !(s[1] == 'x' || s[1] == 'X'))) {
		v = strtoul(s + (s[0] == '/'), &end, 10);
		if (*end || end == s + (s[0] == '/') || v > 32)
			return 0;
		*mask = prefix_mask(v);
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

static int parse_int(const char *s, int *v)
{
	char *end;
	long n = strtol(s, &end, 10);

	if (!*s || *end || n < 0 || n > 1000000)
		return 0;
	*v = (int)n;
	return 1;
}

static void bad(const char *what, const char *arg)
{
	fprintf(stderr, "ifconfig: %s: %s\n", arg, what);
	exit(1);
}

static void setflags(const char *name, unsigned set, unsigned clear)
{
	struct ifreq r;

	setname(&r, name);
	IOC(SIOCGIFFLAGS, &r);
	r.ifr_flags = (short)(((unsigned short)r.ifr_flags | set) & ~clear);
	IOC(SIOCSIFFLAGS, &r);
}

static void params(const char *name, char **av)
{
	uint32_t addr = 0, mask = 0, bcast = 0;
	int have_addr = 0, have_mask = 0, have_bcast = 0, alias = 0, del = 0, mtu = -1, metric = -1;
	unsigned fset = 0, fclear = 0;
	struct ifreq r;

	for (; *av; av++) {
		const char *p = *av;
		char *slash;

		if (!strcmp(p, "inet"))
			continue;
		if (!strcmp(p, "up")) {
			fset |= IFF_UP, fclear &= ~(unsigned)IFF_UP;
		} else if (!strcmp(p, "down")) {
			fclear |= IFF_UP, fset &= ~(unsigned)IFF_UP;
		} else if (!strcmp(p, "debug") || !strcmp(p, "-debug")) {
			if (p[0] == '-')
				fclear |= IFF_DEBUG, fset &= ~(unsigned)IFF_DEBUG;
			else
				fset |= IFF_DEBUG, fclear &= ~(unsigned)IFF_DEBUG;
		} else if ((!strncmp(p, "link", 4) && p[4] >= '0' && p[4] <= '2' && !p[5]) ||
		    (p[0] == '-' && !strncmp(p + 1, "link", 4) && p[5] >= '0' && p[5] <= '2' && !p[6])) {
			unsigned bit = IFF_LINK0 << (p[p[0] == '-' ? 5 : 4] - '0');

			if (p[0] == '-')
				fclear |= bit, fset &= ~bit;
			else
				fset |= bit, fclear &= ~bit;
		} else if (!strcmp(p, "alias")) {
			alias = 1;
		} else if (!strcmp(p, "-alias") || !strcmp(p, "delete")) {
			del = 1;
		} else if (!strcmp(p, "netmask") || !strcmp(p, "broadcast") || !strcmp(p, "mtu") || !strcmp(p, "metric")) {
			if (!av[1])
				bad("needs a value", p);
			av++;
			if (p[0] == 'n') {
				if (!parse_mask(*av, &mask))
					bad("invalid netmask", *av);
				have_mask = 1;
			} else if (p[0] == 'b') {
				if (!parse_ip(*av, &bcast))
					bad("invalid broadcast address", *av);
				have_bcast = 1;
			} else if (p[1] == 't') {
				if (!parse_int(*av, &mtu))
					bad("invalid mtu", *av);
			} else if (!parse_int(*av, &metric))
				bad("invalid metric", *av);
		} else if (!have_addr && (slash = strchr(p, '/')) != NULL) {
			char *tmp = strdup(p);

			if (!tmp)
				bad(strerror(errno), p);
			tmp[slash - p] = 0;
			if (!parse_ip(tmp, &addr) || !parse_mask(slash, &mask))
				bad("invalid address", p);
			free(tmp);
			have_addr = have_mask = 1;
		} else if (!have_addr && parse_ip(p, &addr)) {
			have_addr = 1;
		} else {
			bad("unknown parameter or invalid address", p);
		}
	}

	if (mtu >= 0) {
		setname(&r, name);
		r.ifr_mtu = mtu;
		IOC(SIOCSIFMTU, &r);
	}
	if (metric >= 0) {
		setname(&r, name);
		r.ifr_metric = metric;
		IOC(SIOCSIFMETRIC, &r);
	}
	if (del) {
		setname(&r, name);
		if (!have_addr) {
			IOC(SIOCGIFADDR, &r);
		} else
			put_in(&r.ifr_addr, addr);
		IOC(SIOCDIFADDR, &r);
	} else if (have_addr && alias) {
		struct ifaliasreq ra;

		memset(&ra, 0, sizeof ra);
		if (strlen(name) >= IFNAMSIZ)
			bad("interface name too long", name);
		strcpy(ra.ifra_name, name);
		put_in(&ra.ifra_addr, addr);
		if (have_mask)
			put_in(&ra.ifra_mask, mask);
		IOC(SIOCAIFADDR, &ra);
	} else if (have_addr) {
		setname(&r, name);
		put_in(&r.ifr_addr, addr);
		IOC(SIOCSIFADDR, &r);
	}
	if (have_mask && !(have_addr && alias)) {
		setname(&r, name);
		put_in(&r.ifr_addr, mask);
		IOC(SIOCSIFNETMASK, &r);
	}
	if (have_bcast) {
		setname(&r, name);
		put_in(&r.ifr_addr, bcast);
		IOC(SIOCSIFBRDADDR, &r);
	}
	if (fset || fclear)
		setflags(name, fset, fclear);
}

int main(int argc, char *argv[])
{
	struct ifaddrs *list, *a;
	int all = 0, names = 0, c, found = 0, listed = 0;

	while ((c = getopt(argc, argv, "al")) != -1) {
		switch (c) {
		case 'a':
			all = 1;
			break;
		case 'l':
			names = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0 && (all || names))
		usage();
	if ((sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) == -1) {
		fprintf(stderr, "ifconfig: socket: %s\n", strerror(errno));
		return 1;
	}
	if (argc == 2 && !strcmp(argv[1], "inet")) /* (just the family: show) */
		argc = 1;
	if (argc > 1) {
		if (!if_nametoindex(argv[0])) {
			fprintf(stderr, "ifconfig: interface %s does not exist\n", argv[0]);
			return 1;
		}
		params(argv[0], argv + 1);
		return 0;
	}
	if (getifaddrs(&list) == -1) {
		fprintf(stderr, "ifconfig: getifaddrs: %s\n", strerror(errno));
		return 1;
	}
	for (a = list; a; a = a->ifa_next) {
		if (!a->ifa_addr || a->ifa_addr->sa_family != AF_LINK)
			continue;
		if (argc == 1 && strcmp(argv[0], a->ifa_name))
			continue;
		found = 1;
		if (names)
			printf("%s%s", listed++ ? " " : "", a->ifa_name);
		else
			show(list, a);
	}
	if (names)
		printf("\n");
	else if (!found) {
		fprintf(stderr, "ifconfig: interface %s does not exist\n", argv[0]);
		return 1;
	}
	freeifaddrs(list);
	return fflush(stdout) == 0 ? 0 : 1;
}
