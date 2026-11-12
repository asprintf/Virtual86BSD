/* k_ifconfig.c - the interface and route ioctls (net/if.c, net/route.c),
   getifaddrs() and the routing table dump (sysctl net.route).
   As root, on V86BSD with a network card (em0 or vio0).
 *
 * The interface under test is the first card.  Its own address (DHCP) is
 * put back at the end. */
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); failures++; } } while (0)

static int fd;
static char nic[IFNAMSIZ];

static void put(struct sockaddr *sa, const char *ip)
{
	struct sockaddr_in *s = (struct sockaddr_in *)sa;
	memset(sa, 0, sizeof *sa);
	s->sin_family = AF_INET;
	inet_pton(AF_INET, ip, &s->sin_addr);
}

static const char *get(const struct sockaddr *sa)
{
	return inet_ntoa(((const struct sockaddr_in *)sa)->sin_addr);
}

static void req(struct ifreq *r)
{
	memset(r, 0, sizeof *r);
	strcpy(r->ifr_name, nic);
}

/* the first address, mask, mtu ... of the card as strings */
static int addr_of(unsigned long cmd, char *out)
{
	struct ifreq r;
	req(&r);
	if (ioctl(fd, cmd, &r) == -1)
		return -1;
	strcpy(out, get(&r.ifr_addr));
	return 0;
}

static int setaddr(unsigned long cmd, const char *ip)
{
	struct ifreq r;
	req(&r);
	put(&r.ifr_addr, ip);
	return ioctl(fd, cmd, &r);
}

static int route(unsigned long cmd, const char *dst, const char *mask, const char *gw, int flags, const char *dev, int metric)
{
	struct rtentry rt;
	memset(&rt, 0, sizeof rt);
	put(&rt.rt_dst, dst);
	if (mask)
		put(&rt.rt_genmask, mask);
	if (gw)
		put(&rt.rt_gateway, gw);
	rt.rt_flags = (unsigned short)(RTF_UP | flags);
	rt.rt_dev = (char *)dev;
	rt.rt_metric = (short)metric;
	return ioctl(fd, cmd, &rt);
}

/* does the table (as the dump gives it) have dst/mask [through gw]? */
static struct rtinfo found;
static int in_table(const char *dst, const char *mask, const char *gw)
{
	int mib[6] = { CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };
	size_t len = 0;
	struct rtinfo *t;
	struct in_addr d, m, g;
	int n, i, r = 0;

	inet_pton(AF_INET, dst, &d);
	inet_pton(AF_INET, mask, &m);
	g.s_addr = 0;
	if (gw && gw[0] != '*') /* ("*": any gateway) */
		inet_pton(AF_INET, gw, &g);
	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
		return -1;
	t = malloc(len + sizeof *t);
	len += sizeof *t;
	if (sysctl(mib, 6, t, &len, NULL, 0) == -1)
		return -1;
	n = (int)(len / sizeof *t);
	for (i = 0; i < n; i++)
		if (t[i].ri_dst == d.s_addr && t[i].ri_mask == m.s_addr && (t[i].ri_gateway == g.s_addr || (gw && gw[0] == '*'))) {
			found = t[i];
			r = 1;
		}
	free(t);
	return r;
}

int main(void)
{
	struct ifreq r;
	struct ifaliasreq ra;
	char own[32], ownmask[32], buf[32];
	struct ifaddrs *list, *a;
	char gw0[32] = "";
	int flags0, i;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	CHECK(fd >= 0);
	CHECK(if_indextoname(2, nic) != NULL);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0);
	flags0 = (unsigned short)r.ifr_flags;
	CHECK((flags0 & (IFF_UP | IFF_BROADCAST | IFF_RUNNING)) == (IFF_UP | IFF_BROADCAST | IFF_RUNNING));
	CHECK(!(flags0 & IFF_LOOPBACK));
	CHECK(addr_of(SIOCGIFADDR, own) == 0 && addr_of(SIOCGIFNETMASK, ownmask) == 0);
	printf("%s %s %s\n", nic, own, ownmask);
	if (in_table("0.0.0.0", "0.0.0.0", "*") == 1) /* DHCP's default route: put back at the end */
		inet_ntop(AF_INET, &found.ri_gateway, gw0, sizeof gw0);

	/* the loopback: BSD flag values, its mask, no broadcast */
	memset(&r, 0, sizeof r);
	strcpy(r.ifr_name, "lo0");
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0 && ((unsigned short)r.ifr_flags & (IFF_UP | IFF_LOOPBACK | IFF_MULTICAST)) == (IFF_UP | IFF_LOOPBACK | IFF_MULTICAST));
	CHECK(ioctl(fd, SIOCGIFBRDADDR, &r) == -1 && errno == EINVAL);
	CHECK(ioctl(fd, SIOCGIFMTU, &r) == 0 && r.ifr_mtu == 65536);

	/* errors */
	memset(&r, 0, sizeof r);
	strcpy(r.ifr_name, "xx9");
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == -1 && errno == ENODEV);
	CHECK(setaddr(SIOCSIFADDR, "0.0.0.0") == -1 && errno == EINVAL);
	CHECK(setaddr(SIOCSIFADDR, "255.255.255.255") == -1 && errno == EINVAL);
	CHECK(setaddr(SIOCSIFADDR, "224.0.0.1") == -1 && errno == EINVAL);
	CHECK(setaddr(SIOCSIFADDR, "127.0.0.9") == -1 && errno == EINVAL); /* 127.x only on lo0 */
	CHECK(setaddr(SIOCSIFNETMASK, "255.0.255.0") == -1 && errno == EINVAL); /* not a prefix */
	CHECK(setaddr(SIOCSIFBRDADDR, "1.2.3.4") == -1 && errno == EOPNOTSUPP);
	CHECK(setaddr(SIOCSIFDSTADDR, "1.2.3.4") == -1 && errno == EINVAL);
	req(&r);
	r.ifr_addr.sa_family = AF_UNIX;
	CHECK(ioctl(fd, SIOCSIFADDR, &r) == -1 && errno == EAFNOSUPPORT);

	/* mtu and metric */
	req(&r);
	r.ifr_mtu = 67;
	CHECK(ioctl(fd, SIOCSIFMTU, &r) == -1 && errno == EINVAL);
	r.ifr_mtu = 1501;
	CHECK(ioctl(fd, SIOCSIFMTU, &r) == -1 && errno == EINVAL); /* the card's own is 1500 */
	r.ifr_mtu = 1400;
	CHECK(ioctl(fd, SIOCSIFMTU, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFMTU, &r) == 0 && r.ifr_mtu == 1400);
	r.ifr_mtu = 1500;
	CHECK(ioctl(fd, SIOCSIFMTU, &r) == 0);
	req(&r);
	r.ifr_metric = -1;
	CHECK(ioctl(fd, SIOCSIFMETRIC, &r) == -1 && errno == EINVAL);
	r.ifr_metric = 7;
	CHECK(ioctl(fd, SIOCSIFMETRIC, &r) == 0);
	{ /* the connected route took the metric */
		struct in_addr n, m;
		struct in_addr o;
		inet_pton(AF_INET, own, &o);
		inet_pton(AF_INET, ownmask, &m);
		n.s_addr = o.s_addr & m.s_addr;
		CHECK(in_table(inet_ntoa(n), ownmask, NULL) == 1 && found.ri_metric == 7 && found.ri_index == 2);
	}
	req(&r);
	r.ifr_metric = 0;
	CHECK(ioctl(fd, SIOCSIFMETRIC, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFMETRIC, &r) == 0 && r.ifr_metric == 0);

	/* flags: unsupported ones are refused, DEBUG and LINK0 stick */
	req(&r);
	r.ifr_flags = (short)(flags0 | IFF_PROMISC);
	CHECK(ioctl(fd, SIOCSIFFLAGS, &r) == -1 && errno == EOPNOTSUPP);
	r.ifr_flags = (short)(flags0 | IFF_DEBUG | IFF_LINK0);
	CHECK(ioctl(fd, SIOCSIFFLAGS, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0 && ((unsigned short)r.ifr_flags & (IFF_DEBUG | IFF_LINK0 | IFF_UP)) == (IFF_DEBUG | IFF_LINK0 | IFF_UP));
	r.ifr_flags = (short)flags0;
	CHECK(ioctl(fd, SIOCSIFFLAGS, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0 && (unsigned short)r.ifr_flags == (unsigned)flags0);

	/* aliases: add with a mask, the connected route comes, the first
	   address stays first, getifaddrs() has it with its own mask */
	memset(&ra, 0, sizeof ra);
	strcpy(ra.ifra_name, nic);
	put(&ra.ifra_addr, "10.77.0.5");
	put(&ra.ifra_mask, "255.255.0.0");
	CHECK(ioctl(fd, SIOCAIFADDR, &ra) == 0);
	CHECK(in_table("10.77.0.0", "255.255.0.0", NULL) == 1);
	CHECK(addr_of(SIOCGIFADDR, buf) == 0 && !strcmp(buf, own));
	if (strcmp(buf, own))
		printf("first address is %s, not %s\n", buf, own);
	put(&ra.ifra_addr, "10.78.1.9"); /* no mask: its class' (A) */
	memset(&ra.ifra_mask, 0, sizeof ra.ifra_mask);
	CHECK(ioctl(fd, SIOCAIFADDR, &ra) == 0);
	CHECK(in_table("10.0.0.0", "255.0.0.0", NULL) == 1);
	put(&ra.ifra_addr, "10.77.0.5"); /* it exists: no error, the same mask */
	CHECK(ioctl(fd, SIOCAIFADDR, &ra) == 0);
	req(&r);
	put(&r.ifr_addr, "10.77.0.5");
	CHECK(ioctl(fd, SIOCGIFNETMASK, &r) == 0 && !strcmp(get(&r.ifr_addr), "255.255.0.0"));
	req(&r);
	put(&r.ifr_addr, "10.78.1.9");
	CHECK(ioctl(fd, SIOCGIFBRDADDR, &r) == 0 && !strcmp(get(&r.ifr_addr), "10.255.255.255"));
	CHECK(getifaddrs(&list) == 0);
	{
		int link = 0, seen77 = 0, seen78 = 0, lo = 0;
		for (a = list; a; a = a->ifa_next) {
			if (a->ifa_addr && a->ifa_addr->sa_family == AF_LINK && !strcmp(a->ifa_name, nic)) {
				struct sockaddr_dl *dl = (struct sockaddr_dl *)a->ifa_addr;
				link = dl->sdl_alen == 6 && dl->sdl_index == 2 && dl->sdl_type == 6 && (a->ifa_flags & IFF_UP);
			}
			if (a->ifa_addr && a->ifa_addr->sa_family == AF_LINK && !strcmp(a->ifa_name, "lo0")) {
				struct sockaddr_dl *dl = (struct sockaddr_dl *)a->ifa_addr;
				lo = dl->sdl_alen == 0 && dl->sdl_index == 1 && dl->sdl_type == 0x18;
			}
			if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || strcmp(a->ifa_name, nic))
				continue;
			if (!strcmp(get(a->ifa_addr), "10.77.0.5"))
				seen77 = a->ifa_netmask && !strcmp(get(a->ifa_netmask), "255.255.0.0") && a->ifa_dstaddr && !strcmp(get(a->ifa_dstaddr), "10.77.255.255");
			if (!strcmp(get(a->ifa_addr), "10.78.1.9"))
				seen78 = a->ifa_netmask && !strcmp(get(a->ifa_netmask), "255.0.0.0");
		}
		CHECK(link && lo && seen77 && seen78);
	}
	freeifaddrs(list);
	/* the same address on another interface is refused */
	memset(&ra, 0, sizeof ra);
	strcpy(ra.ifra_name, "lo0");
	put(&ra.ifra_addr, "10.77.0.5");
	CHECK(ioctl(fd, SIOCAIFADDR, &ra) == -1 && errno == EADDRINUSE);
	/* delete: the address and its route go (the alias on the same
	   network keeps its own) */
	req(&r);
	put(&r.ifr_addr, "10.77.0.5");
	CHECK(ioctl(fd, SIOCDIFADDR, &r) == 0);
	CHECK(in_table("10.77.0.0", "255.255.0.0", NULL) == 0);
	CHECK(ioctl(fd, SIOCDIFADDR, &r) == -1 && errno == EADDRNOTAVAIL);
	put(&r.ifr_addr, "10.78.1.9");
	CHECK(ioctl(fd, SIOCDIFADDR, &r) == 0);
	CHECK(in_table("10.0.0.0", "255.0.0.0", NULL) == 0);

	/* the first address: replace it (the mask stays), set a mask */
	CHECK(setaddr(SIOCSIFADDR, "10.99.0.2") == 0);
	CHECK(addr_of(SIOCGIFADDR, buf) == 0 && !strcmp(buf, "10.99.0.2"));
	CHECK(addr_of(SIOCGIFNETMASK, buf) == 0 && !strcmp(buf, ownmask));
	CHECK(setaddr(SIOCSIFNETMASK, "255.255.255.128") == 0);
	CHECK(in_table("10.99.0.0", "255.255.255.128", NULL) == 1);
	CHECK(addr_of(SIOCGIFBRDADDR, buf) == 0 && !strcmp(buf, "10.99.0.127"));
	CHECK(setaddr(SIOCSIFBRDADDR, "10.99.0.127") == 0);

	/* routes: through a gateway on the network, host, default, metric */
	CHECK(route(SIOCADDRT, "172.16.0.0", "255.240.0.0", "10.99.0.1", RTF_GATEWAY, NULL, 0) == 0);
	CHECK(in_table("172.16.0.0", "255.240.0.0", "10.99.0.1") == 1 && (found.ri_flags & RTF_GATEWAY) && found.ri_index == 2);
	CHECK(route(SIOCADDRT, "172.16.0.0", "255.240.0.0", "10.99.0.1", RTF_GATEWAY, NULL, 0) == -1 && errno == EEXIST);
	CHECK(route(SIOCADDRT, "172.20.0.0", "255.255.0.0", "10.200.0.1", RTF_GATEWAY, NULL, 0) == -1 && errno == ENETUNREACH);
	CHECK(route(SIOCADDRT, "172.21.0.0", "255.255.0.0", NULL, 0, NULL, 0) == -1 && errno == ENODEV); /* on which link? */
	CHECK(route(SIOCADDRT, "172.21.0.0", "255.255.0.0", NULL, 0, "nope0", 0) == -1 && errno == ENODEV);
	CHECK(route(SIOCADDRT, "172.22.0.0", "255.255.0.0", NULL, 0, nic, 0) == 0);
	CHECK(in_table("172.22.0.0", "255.255.0.0", NULL) == 1 && found.ri_gateway == 0);
	CHECK(route(SIOCADDRT, "192.0.2.7", NULL, "10.99.0.1", RTF_GATEWAY | RTF_HOST, NULL, 3) == 0); /* /32 by the flag */
	CHECK(in_table("192.0.2.7", "255.255.255.255", "10.99.0.1") == 1 && found.ri_metric == 2 && (found.ri_flags & RTF_HOST));
	CHECK(route(SIOCADDRT, "192.0.2.8", "255.255.255.0", "10.99.0.1", RTF_GATEWAY | RTF_HOST, NULL, 0) == -1 && errno == EINVAL);
	CHECK(route(SIOCADDRT, "0.0.0.0", "0.0.0.0", "10.99.0.1", RTF_GATEWAY, NULL, 0) == 0);
	CHECK(in_table("0.0.0.0", "0.0.0.0", "10.99.0.1") == 1);
	CHECK(route(SIOCADDRT, "9.9.9.0", NULL, "0.0.0.0", RTF_GATEWAY, NULL, 0) == -1 && errno == EINVAL); /* no gateway given */
	/* taking the address away takes the routes through it away */
	req(&r);
	put(&r.ifr_addr, "10.99.0.2");
	CHECK(ioctl(fd, SIOCDIFADDR, &r) == 0);
	CHECK(in_table("0.0.0.0", "0.0.0.0", "10.99.0.1") == 0);
	CHECK(in_table("172.16.0.0", "255.240.0.0", "10.99.0.1") == 0);
	CHECK(in_table("10.99.0.0", "255.255.255.128", NULL) == 0);
	CHECK(route(SIOCDELRT, "172.22.0.0", "255.255.0.0", NULL, 0, NULL, 0) == 0);
	CHECK(route(SIOCDELRT, "172.22.0.0", "255.255.0.0", NULL, 0, NULL, 0) == -1 && errno == ESRCH);

	/* down: the interface keeps its address and routes, but IFF_RUNNING goes */
	CHECK(setaddr(SIOCSIFADDR, own) == 0);
	CHECK(setaddr(SIOCSIFNETMASK, ownmask) == 0);
	req(&r);
	r.ifr_flags = (short)(flags0 & ~IFF_UP);
	CHECK(ioctl(fd, SIOCSIFFLAGS, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0 && !((unsigned short)r.ifr_flags & (IFF_UP | IFF_RUNNING)));
	CHECK(addr_of(SIOCGIFADDR, buf) == 0 && !strcmp(buf, own));
	r.ifr_flags = (short)flags0;
	CHECK(ioctl(fd, SIOCSIFFLAGS, &r) == 0);
	req(&r);
	CHECK(ioctl(fd, SIOCGIFFLAGS, &r) == 0 && (unsigned short)r.ifr_flags == (unsigned)flags0);

	/* everything is back as it was (but the default route) */
	CHECK(addr_of(SIOCGIFADDR, buf) == 0 && !strcmp(buf, own));
	CHECK(addr_of(SIOCGIFNETMASK, buf) == 0 && !strcmp(buf, ownmask));

	/* the numbers other programs see: SIOCGIFCONF lists every address */
	{
		struct ifconf ic;
		struct ifreq conf[8];
		int n = 0;
		ic.ifc_len = 0;
		ic.ifc_buf = NULL;
		CHECK(ioctl(fd, SIOCGIFCONF, &ic) == 0 && ic.ifc_len >= (int)(2 * sizeof(struct ifreq)));
		ic.ifc_len = sizeof conf;
		ic.ifc_req = conf;
		CHECK(ioctl(fd, SIOCGIFCONF, &ic) == 0);
		for (i = 0; i < ic.ifc_len / (int)sizeof(struct ifreq); i++)
			if (!strcmp(conf[i].ifr_name, "lo0") && !strcmp(get(&conf[i].ifr_addr), "127.0.0.1"))
				n++;
		CHECK(n == 1);
	}

	/* DHCP's default route went with the network it was on: back */
	if (gw0[0]) {
		CHECK(in_table("0.0.0.0", "0.0.0.0", "*") == 0);
		CHECK(route(SIOCADDRT, "0.0.0.0", "0.0.0.0", gw0, RTF_GATEWAY, NULL, 0) == 0);
	}
	if (failures)
		return 1;
	printf("k_ifconfig ok\n");
	return 0;
}
