/* t_inet.c - <arpa/inet.h>, <netdb.h>, <net/if.h>, <netinet/in.h> against
 * the system's library: address conversions (random and edge cases),
 * getaddrinfo/getnameinfo on numeric names and /etc/hosts, the
 * services, protocols, networks and hosts databases, interfaces. */
#define _XOPEN_SOURCE 800
#include <arpa/inet.h>
#include <ctype.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static unsigned long long seed = 88172645463325252ULL;
static unsigned rnd(void)
{
	seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
	return (unsigned)(seed >> 11);
}

static void pton(int af, const char *s)
{
	unsigned char a[16];
	char out[64];
	int r = inet_pton(af, s, a);
	printf("pton(%d, \"%s\") = %d", af, s, r);
	if (r == 1)
		printf(" -> %s", inet_ntop(af, a, out, sizeof out));
	printf("\n");
}

static void conversions(void)
{
	static const char *v4[] = {"0.0.0.0", "255.255.255.255", "1.2.3.4", "01.2.3.4", "1.2.3", "1.2.3.4.5", "256.1.1.1",
		"1..2.3", "1.2.3.4 ", " 1.2.3.4", "", "a.b.c.d", "1.2.3.04", "10.0.0.1x", "127.1", "0x7f.1"};
	static const char *v6[] = {"::", "::1", "1::", "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8:9", "1::2::3", ":::",
		"1:2:3:4:5:6:7::", "::1:2:3:4:5:6:7", "fe80::1%eth0", "::ffff:1.2.3.4", "::1.2.3.4", "1::1.2.3.4",
		"12345::", "1:2:3:4:5:6:1.2.3.4", "1:2:3:4:5:6:7:1.2.3.4", "::ffff:1.2.3", "FFFF::", "0:0:0:0:0:0:0:0",
		"1:0:0:1:0:0:0:1", "1:0:0:0:1:0:0:1", "0:1:0:0:0:0:0:0", "1:", ":1", "1:2", "::0.0.0.1", "::0.1.0.0",
		"::ffff:0:0", "2001:db8::0:1", "1:2:3::", "::2:3:4"};
	static const char *addr[] = {"1.2.3.4", "127.1", "1.2", "0x7f000001", "017700000001", "1.2.3.256", "1.2.65536",
		"255.255.255.255", "08.1.1.1", "0x1.0x2.0x3.0x4", "1.2.3.4.", "", " 1", "4294967295", "4294967296",
		"0", "1.0xffffff", "1.0x1000000"};
	char out[64];
	size_t i;
	int k;
	for (i = 0; i < sizeof v4 / sizeof *v4; i++)
		pton(AF_INET, v4[i]);
	for (i = 0; i < sizeof v6 / sizeof *v6; i++)
		pton(AF_INET6, v6[i]);
	for (i = 0; i < sizeof addr / sizeof *addr; i++)
		printf("inet_addr(\"%s\") = %08x\n", addr[i], (unsigned)inet_addr(addr[i]));
	/* random addresses, with runs of zeros and v4 tails */
	for (k = 0; k < 20000; k++) {
		unsigned char a[16], b[16];
		int j;
		for (j = 0; j < 16; j += 2) {
			unsigned r = rnd();
			a[j] = r & 4 ? (unsigned char)r : 0;
			a[j + 1] = r & 8 ? (unsigned char)(r >> 8) : 0;
		}
		if (k % 7 == 0) { memset(a, 0, 10); a[10] = a[11] = 0xff; }
		if (k % 11 == 0) memset(a, 0, 12);
		inet_ntop(AF_INET6, a, out, sizeof out);
		printf("%s", out);
		if (inet_pton(AF_INET6, out, b) != 1 || memcmp(a, b, 16))
			printf(" (no round trip)");
		printf(" %s\n", inet_ntop(AF_INET, a, out, sizeof out));
	}
	{
		struct in_addr ia;
		ia.s_addr = htonl(0xc0a80101);
		printf("%s %08x %04x %08x %04x\n", inet_ntoa(ia), htonl(0x11223344), htons(0x1122), ntohl(0x11223344), ntohs(0x1122));
		printf("ntop small: %p\n", (void *)inet_ntop(AF_INET, &ia, out, 7));
		printf("pton af: %d\n", inet_pton(12345, "1.2.3.4", out));
	}
}

static int lcmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static void gai(const char *n, const char *s, int fam, int st, int pr, int fl)
{
	struct addrinfo h, *r, *p;
	int e;
	/* the entries of one answer, sorted: their order is the system's (the
	 * RFC 6724 one puts ::1 before 127.0.0.1 where IPv6 is up) */
	static char lines[64][400];
	size_t nl = 0, li;
	memset(&h, 0, sizeof h);
	h.ai_family = fam;
	h.ai_socktype = st;
	h.ai_protocol = pr;
	h.ai_flags = fl;
	e = getaddrinfo(n, s, &h, &r);
	printf("gai(%s, %s, %d, %d, %d, %#x) = %d %s\n", n ? n : "NULL", s ? s : "NULL", fam, st, pr, fl, e, e ? gai_strerror(e) : "");
	if (e)
		return;
	for (p = r; p; p = p->ai_next) {
		char b[64], hb[100], sb[32];
		/* glibc 2.4x also returns SCTP (132) entries for a service; V86BSD has no SCTP */
		if (p->ai_protocol == 132)
			continue;
		void *a = p->ai_family == AF_INET ? (void *)&((struct sockaddr_in *)p->ai_addr)->sin_addr
		                                  : (void *)&((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
		e = getnameinfo(p->ai_addr, p->ai_addrlen, hb, sizeof hb, sb, sizeof sb, NI_NUMERICHOST | NI_NUMERICSERV);
		/* host names are case-insensitive: systemd-resolved's glibc answers
		 * "LocalHost" for a query of that spelling, /etc/hosts says "localhost" */
		char cn[256] = "-";
		if (p->ai_canonname) {
			size_t q;
			for (q = 0; p->ai_canonname[q] && q < sizeof cn - 1; q++)
				cn[q] = (char)tolower((unsigned char)p->ai_canonname[q]), cn[q + 1] = 0;
		}
		if (nl < 64)
			snprintf(lines[nl++], sizeof lines[0], "  %d %d %d len=%u %s port=%d canon=%s gni=%d %s %s\n",
			         p->ai_family, p->ai_socktype, p->ai_protocol,
			         (unsigned)p->ai_addrlen, inet_ntop(p->ai_family, a, b, sizeof b),
			         ntohs(((struct sockaddr_in *)p->ai_addr)->sin_port), cn,
			         e, hb, sb);
	}
	qsort(lines, nl, sizeof lines[0], lcmp);
	for (li = 0; li < nl; li++)
		fputs(lines[li], stdout);
	freeaddrinfo(r);
}

static void gni4(unsigned ip, int port, int flags, socklen_t hl, socklen_t sl)
{
	struct sockaddr_in sa;
	char h[256], s[64];
	int e;
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short)port);
	sa.sin_addr.s_addr = htonl(ip);
	e = getnameinfo((struct sockaddr *)&sa, sizeof sa, hl ? h : 0, hl, sl ? s : 0, sl, flags);
	printf("gni(%08x, %d, %#x, %u, %u) = %d %s %s\n", ip, port, flags, (unsigned)hl, (unsigned)sl, e,
	       !e && hl ? h : "", !e && sl ? s : "");
}

static void lookups(void)
{
	static const char *serv[] = {0, "80", "http", "domain", "ssh", "nosuch", "0", "65535", "65536", "-1", "0x10", "smtp"};
	static const int st[] = {0, SOCK_STREAM, SOCK_DGRAM, SOCK_RAW};
	static const char *nodes[] = {"localhost", "LocalHost", "127.0.0.1", "127.1", "::1", "::ffff:127.0.0.1", "0", "vm",
		"1.2.3.4", "fe80::1%lo", "fe80::1%1", "fe80::1%nosuch", "::", "0.0.0.0", "2001:db8::1"};
	size_t i, j, k;
	for (i = 0; i < sizeof serv / sizeof *serv; i++)
		for (j = 0; j < 4; j++)
			gai("localhost", serv[i], AF_UNSPEC, st[j], 0, 0);
	for (i = 0; i < sizeof nodes / sizeof *nodes; i++)
		for (k = 0; k < 3; k++) {
			static const int fam[] = {AF_UNSPEC, AF_INET, AF_INET6};
			gai(nodes[i], "22", fam[k], SOCK_STREAM, 0, AI_CANONNAME);
			gai(nodes[i], 0, fam[k], SOCK_DGRAM, 0, AI_V4MAPPED);
			gai(nodes[i], 0, fam[k], SOCK_STREAM, 0, AI_V4MAPPED | AI_ALL);
			gai(nodes[i], 0, fam[k], SOCK_STREAM, 0, AI_NUMERICHOST | AI_ADDRCONFIG);
		}
	gai(0, "80", AF_UNSPEC, 0, 0, 0);
	gai(0, "80", AF_UNSPEC, 0, 0, AI_PASSIVE);
	gai(0, "80", AF_INET6, SOCK_STREAM, 0, AI_PASSIVE);
	gai(0, 0, AF_UNSPEC, 0, 0, 0);
	gai(0, "80", AF_UNSPEC, 0, 0, AI_CANONNAME);
	gai("localhost", 0, 99, 0, 0, 0);
	gai("localhost", 0, AF_UNSPEC, 99, 0, 0);
	gai("localhost", 0, AF_UNSPEC, 0, 0, 0x10000);
	gai("localhost", 0, AF_UNSPEC, 0, IPPROTO_UDP, 0);
	gai("localhost", "domain", AF_UNSPEC, 0, IPPROTO_TCP, 0);
	gai("1.2.3.4", "x", AF_UNSPEC, 0, 0, AI_NUMERICSERV);
	gai("foo", 0, AF_UNSPEC, 0, 0, AI_NUMERICHOST);

	gni4(0x7f000001, 80, 0, 64, 32);
	gni4(0x7f000001, 53, NI_DGRAM, 64, 32);
	gni4(0x7f000001, 80, NI_NUMERICHOST | NI_NUMERICSERV, 64, 32);
	gni4(0x7f000001, 80, NI_NOFQDN, 64, 32);
	gni4(0x7f000001, 80, 0, 5, 32);
	gni4(0x7f000001, 80, 0, 64, 2);
	gni4(0x7f000001, 80, 0, 0, 32);
	gni4(0x7f000001, 80, 0, 64, 0);
	gni4(0x7f000001, 12345, 0, 64, 32);
	gni4(0x7f000001, 80, 0x1000, 64, 32);
	{
		struct sockaddr_in6 s6;
		char h[100], s[32];
		memset(&s6, 0, sizeof s6);
		s6.sin6_family = AF_INET6;
		s6.sin6_port = htons(22);
		inet_pton(AF_INET6, "fe80::1", &s6.sin6_addr);
		s6.sin6_scope_id = 1;
		printf("gni6 %d", getnameinfo((struct sockaddr *)&s6, sizeof s6, h, sizeof h, s, sizeof s, NI_NUMERICHOST));
		printf(" %s %s\n", h, s);
		printf("gni6 bad len %d\n", getnameinfo((struct sockaddr *)&s6, 8, h, sizeof h, s, sizeof s, NI_NUMERICHOST));
		s6.sin6_family = 12345;
		printf("gni bad family %d\n", getnameinfo((struct sockaddr *)&s6, sizeof s6, h, sizeof h, s, sizeof s, 0));
	}
	for (i = 0; i <= 13; i++)
		printf("%d: %s\n", -(int)i, gai_strerror(-(int)i));
}

static void databases(void)
{
	struct servent *se;
	struct protoent *pe;
	struct netent *ne;
	struct hostent *he;
	static const char *sn[] = {"http", "www", "domain", "nosuch", "ssh", "mail", "time"};
	static const char *pr[] = {0, "tcp", "udp", "ddp"};
	static const int ports[] = {80, 53, 22, 9, 12345, 37};
	size_t i, j;
	char **a;
	int n = 0;
	for (i = 0; i < sizeof sn / sizeof *sn; i++)
		for (j = 0; j < 4; j++) {
			se = getservbyname(sn[i], pr[j]);
			printf("getservbyname(%s, %s): %s %d %s\n", sn[i], pr[j] ? pr[j] : "NULL", se ? se->s_name : "-",
			       se ? ntohs((unsigned short)se->s_port) : -1, se ? se->s_proto : "-");
		}
	for (i = 0; i < sizeof ports / sizeof *ports; i++)
		for (j = 0; j < 4; j++) {
			se = getservbyport(htons((unsigned short)ports[i]), pr[j]);
			printf("getservbyport(%d, %s): %s %s\n", ports[i], pr[j] ? pr[j] : "NULL", se ? se->s_name : "-", se ? se->s_proto : "-");
		}
	setservent(1);
	while ((se = getservent()) && n++ < 100000) {
		printf("serv %s %d %s", se->s_name, ntohs((unsigned short)se->s_port), se->s_proto);
		for (a = se->s_aliases; *a; a++)
			printf(" %s", *a);
		printf("\n");
	}
	endservent();
	while ((pe = getprotoent())) {
		printf("proto %s %d", pe->p_name, pe->p_proto);
		for (a = pe->p_aliases; *a; a++)
			printf(" %s", *a);
		printf("\n");
	}
	endprotoent();
	pe = getprotobyname("udp");
	printf("udp %d\n", pe ? pe->p_proto : -1);
	pe = getprotobyname("TCP");
	printf("TCP %d\n", pe ? pe->p_proto : -1);
	pe = getprotobynumber(1);
	printf("1 %s\n", pe ? pe->p_name : "-");
	pe = getprotobynumber(9999);
	printf("9999 %s\n", pe ? pe->p_name : "-");
	while ((ne = getnetent()))
		printf("net %s %08x %d\n", ne->n_name, (unsigned)ne->n_net, ne->n_addrtype);
	endnetent();
	ne = getnetbyname("link-local");
	printf("link-local %08x\n", ne ? (unsigned)ne->n_net : 0);
	ne = getnetbyaddr(0xa9fe0000, AF_INET);
	printf("a9fe0000 %s\n", ne ? ne->n_name : "-");
	sethostent(0);
	{
		/* IPv4 lines only, and a line that repeats the one before is
		 * left out: glibc's gethostent() answers an IPv6 line of
		 * /etc/hosts ("::1 localhost") with the IPv4 address of the name */
		char prev[512] = "", cur[512];
		while ((he = gethostent())) {
			char b[64];
			int len;
			if (he->h_addrtype != AF_INET)
				continue;
			len = snprintf(cur, sizeof cur, "host %s %d %d %s", he->h_name, he->h_addrtype, he->h_length,
			               inet_ntop(he->h_addrtype, he->h_addr_list[0], b, sizeof b));
			for (a = he->h_aliases; *a && len > 0 && (size_t)len < sizeof cur; a++)
				len += snprintf(cur + len, sizeof cur - (size_t)len, " %s", *a);
			if (!strcmp(cur, prev))
				continue;
			printf("%s\n", cur);
			strcpy(prev, cur);
		}
	}
	endhostent();
}

static void interfaces(void)
{
	struct if_nameindex *l = if_nameindex(), *p;
	char name[IF_NAMESIZE];
	unsigned lo = if_nametoindex("lo");
	printf("lo %u %s\n", lo, if_indextoname(lo, name) ? name : "-");
	printf("nosuch %u %p\n", if_nametoindex("nosuch"), (void *)if_indextoname(99999, name));
	for (p = l; p && p->if_name; p++)
		printf("if %u %s\n", p->if_index, p->if_name);
	if (l)
		if_freenameindex(l);
}

int main(void)
{
	conversions();
	lookups();
	databases();
	interfaces();
	return 0;
}
