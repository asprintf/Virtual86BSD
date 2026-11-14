/* k_inet: TCP and UDP over the loopback interface, IPv4 and IPv6. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); fails++; } } while (0)

static struct sockaddr_in v4(const char *a, int port)
{
	struct sockaddr_in s;
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	s.sin_port = htons((unsigned short)port);
	inet_pton(AF_INET, a, &s.sin_addr);
	return s;
}

static struct sockaddr_in6 v6(const char *a, int port)
{
	struct sockaddr_in6 s;
	memset(&s, 0, sizeof s);
	s.sin6_family = AF_INET6;
	s.sin6_port = htons((unsigned short)port);
	inet_pton(AF_INET6, a, &s.sin6_addr);
	return s;
}

static int port_of(int fd)
{
	struct sockaddr_in6 s;
	socklen_t l = sizeof s;
	getsockname(fd, (struct sockaddr *)&s, &l);
	return ntohs(s.sin6_port);       /* (same place in sockaddr_in) */
}

int main(void)
{
	int l, c, a, u1, u2, v = 1, p;
	char buf[256], txt[64];
	socklen_t sl;
	struct sockaddr_in sa, pa;
	struct sockaddr_in6 s6;

	signal(SIGPIPE, SIG_IGN);

	/* TCP over IPv4: a listener on the wildcard, port chosen by the system */
	l = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("0.0.0.0", 0);
	CHECK(bind(l, (struct sockaddr *)&sa, sizeof sa) == 0);
	CHECK(listen(l, 4) == 0);
	p = port_of(l);
	CHECK(p >= 1024);
	c = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("127.0.0.1", p);
	CHECK(connect(c, (struct sockaddr *)&sa, sizeof sa) == 0);
	CHECK(connect(c, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EISCONN);
	sl = sizeof pa;
	a = accept(l, (struct sockaddr *)&pa, &sl);
	CHECK(a >= 0 && sl == sizeof pa && pa.sin_family == AF_INET && pa.sin_addr.s_addr == htonl(0x7f000001));
	CHECK(ntohs(pa.sin_port) == port_of(c));
	sl = sizeof pa;
	CHECK(getsockname(a, (struct sockaddr *)&pa, &sl) == 0 && ntohs(pa.sin_port) == p &&
	      pa.sin_addr.s_addr == htonl(0x7f000001));
	sl = sizeof pa;
	CHECK(getsockname(c, (struct sockaddr *)&pa, &sl) == 0 && pa.sin_addr.s_addr == htonl(0x7f000001));
	sl = sizeof pa;
	CHECK(getpeername(c, (struct sockaddr *)&pa, &sl) == 0 && ntohs(pa.sin_port) == p);
	CHECK(write(c, "hello", 5) == 5 && read(a, buf, sizeof buf) == 5 && !memcmp(buf, "hello", 5));
	CHECK(write(a, "back", 4) == 4 && read(c, buf, sizeof buf) == 4);
	/* another address of 127/8 reaches the wildcard listener */
	{
		int c2 = socket(AF_INET, SOCK_STREAM, 0), a2;
		sa = v4("127.1.2.3", p);
		CHECK(connect(c2, (struct sockaddr *)&sa, sizeof sa) == 0);
		sl = sizeof pa;
		a2 = accept(l, (struct sockaddr *)&pa, &sl);
		CHECK(a2 >= 0 && pa.sin_addr.s_addr == inet_addr("127.1.2.3"));
		close(a2);
		close(c2);
	}
	/* end of file; then writing to the closed peer is EPIPE */
	close(a);
	CHECK(read(c, buf, sizeof buf) == 0);
	if (write(c, "x", 1) == 1)       /* (Linux: once, before the reset) */
		(void)0;
	CHECK(write(c, "x", 1) == -1 && (errno == EPIPE || errno == ECONNRESET));
	close(c);

	/* closed with a zero linger: the peer's read fails with ECONNRESET */
	{
		struct linger g = { 1, 0 };
		struct sockaddr_in sb = v4("127.0.0.1", p);
		int c3 = socket(AF_INET, SOCK_STREAM, 0), a3;
		CHECK(connect(c3, (struct sockaddr *)&sb, sizeof sb) == 0);
		a3 = accept(l, NULL, NULL);
		CHECK(a3 >= 0);
		CHECK(setsockopt(a3, SOL_SOCKET, SO_LINGER, &g, sizeof g) == 0);
		close(a3);
		CHECK(read(c3, buf, sizeof buf) == -1 && errno == ECONNRESET);
		close(c3);
	}

	/* the port is taken; SO_REUSEADDR does not get round a listener */
	u1 = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("127.0.0.1", p);
	CHECK(bind(u1, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EADDRINUSE);
	setsockopt(u1, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
	setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
	CHECK(bind(u1, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EADDRINUSE);
	close(u1);
	close(l);
	/* closed: nobody listens */
	c = socket(AF_INET, SOCK_STREAM, 0);
	CHECK(connect(c, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == ECONNREFUSED);
	close(c);
	/* addresses not on this system: no binding to them; no route to them
	   without a network card */
	c = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("192.0.2.1", 80);
	CHECK(bind(c, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EADDRNOTAVAIL);
	if (!if_indextoname(2, txt)) /* (no card: no interface after lo0) */
		CHECK(connect(c, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == ENETUNREACH);
	close(c);

	/* IPv6: a dual-stack listener takes IPv4 clients (as ::ffff:127.0.0.1) */
	l = socket(AF_INET6, SOCK_STREAM, 0);
	s6 = v6("::", 0);
	CHECK(bind(l, (struct sockaddr *)&s6, sizeof s6) == 0 && listen(l, 4) == 0);
	p = port_of(l);
	c = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("127.0.0.1", p);
	CHECK(connect(c, (struct sockaddr *)&sa, sizeof sa) == 0);
	sl = sizeof s6;
	a = accept(l, (struct sockaddr *)&s6, &sl);
	CHECK(a >= 0 && sl == sizeof s6 && s6.sin6_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&s6.sin6_addr));
	close(a);
	close(c);
	c = socket(AF_INET6, SOCK_STREAM, 0);
	s6 = v6("::1", p);
	CHECK(connect(c, (struct sockaddr *)&s6, sizeof s6) == 0);
	sl = sizeof s6;
	a = accept(l, (struct sockaddr *)&s6, &sl);
	CHECK(a >= 0 && IN6_IS_ADDR_LOOPBACK(&s6.sin6_addr));
	inet_ntop(AF_INET6, &s6.sin6_addr, txt, sizeof txt);
	CHECK(!strcmp(txt, "::1"));
	close(a);
	close(c);
	/* an IPv4 socket may share the port with a v6-only listener */
	u1 = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("0.0.0.0", p);
	CHECK(bind(u1, (struct sockaddr *)&sa, sizeof sa) == -1 && errno == EADDRINUSE);
	close(l);
	l = socket(AF_INET6, SOCK_STREAM, 0);
	CHECK(setsockopt(l, IPPROTO_IPV6, IPV6_V6ONLY, &v, sizeof v) == 0);
	s6 = v6("::", p);
	CHECK(bind(l, (struct sockaddr *)&s6, sizeof s6) == 0);
	CHECK(bind(u1, (struct sockaddr *)&sa, sizeof sa) == 0);
	close(u1);
	close(l);

	/* a big transfer between processes, with poll */
	l = socket(AF_INET, SOCK_STREAM, 0);
	sa = v4("127.0.0.1", 0);
	bind(l, (struct sockaddr *)&sa, sizeof sa);
	listen(l, 1);
	p = port_of(l);
	pid_t pid = fork();
	if (pid == 0) {
		int k = socket(AF_INET, SOCK_STREAM, 0);
		char b[8192];
		long sum = 0;
		struct sockaddr_in t = v4("127.0.0.1", p);
		if (connect(k, (struct sockaddr *)&t, sizeof t))
			_exit(2);
		memset(b, 7, sizeof b);
		for (int i = 0; i < 512; i++)
			if (write(k, b, sizeof b) != sizeof b)
				_exit(3);
		shutdown(k, SHUT_WR);
		ssize_t n;
		while ((n = read(k, b, sizeof b)) > 0)
			sum += n;
		_exit(sum == 3 ? 0 : 4);
	}
	{
		struct pollfd pf = {l, POLLIN, 0};
		CHECK(poll(&pf, 1, 5000) == 1 && (pf.revents & POLLIN));
		a = accept(l, 0, 0);
		long total = 0, bad = 0;
		ssize_t n;
		while ((n = read(a, buf, sizeof buf)) > 0) {
			for (ssize_t i = 0; i < n; i++)
				bad += buf[i] != 7;
			total += n;
		}
		CHECK(total == 512 * 8192 && bad == 0);
		CHECK(write(a, "end", 3) == 3);
		close(a);
		int st;
		waitpid(pid, &st, 0);
		CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
	/* non-blocking accept */
	fcntl(l, F_SETFL, O_NONBLOCK);
	CHECK(accept(l, 0, 0) == -1 && errno == EAGAIN);
	close(l);

	/* UDP: unconnected, connected (only its peer gets through), refused */
	u1 = socket(AF_INET, SOCK_DGRAM, 0);
	u2 = socket(AF_INET, SOCK_DGRAM, 0);
	sa = v4("127.0.0.1", 0);
	bind(u1, (struct sockaddr *)&sa, sizeof sa);
	sa.sin_port = htons((unsigned short)port_of(u1));
	CHECK(sendto(u2, "one", 3, 0, (struct sockaddr *)&sa, sizeof sa) == 3);
	sl = sizeof pa;
	CHECK(recvfrom(u1, buf, sizeof buf, 0, (struct sockaddr *)&pa, &sl) == 3 && ntohs(pa.sin_port) == port_of(u2));
	{
		int u3 = socket(AF_INET, SOCK_DGRAM, 0);
		CHECK(connect(u1, (struct sockaddr *)&pa, sizeof pa) == 0);   /* to u2 */
		CHECK(sendto(u3, "no", 2, 0, (struct sockaddr *)&sa, sizeof sa) == 2);
		CHECK(sendto(u2, "yes", 3, 0, (struct sockaddr *)&sa, sizeof sa) == 3);
		CHECK(recv(u1, buf, sizeof buf, MSG_DONTWAIT) == 3 && !memcmp(buf, "yes", 3));
		CHECK(recv(u1, buf, sizeof buf, MSG_DONTWAIT) == -1 && errno == EAGAIN);
		close(u3);
	}
	close(u2);                       /* u1's peer is gone */
	CHECK(send(u1, "?", 1, 0) == 1);
	CHECK(recv(u1, buf, sizeof buf, MSG_DONTWAIT) == -1 && errno == ECONNREFUSED);
	close(u1);
	u1 = socket(AF_INET, SOCK_DGRAM, 0);
	CHECK(send(u1, "?", 1, 0) == -1 && errno == EDESTADDRREQ);
	close(u1);
	/* the address connect() chose is chosen again by the next connect():
	   from the loopback to 192.0.2.1 (TEST-NET-1) not 127.0.0.1 (when
	   there is a network to reach it through), and gone on AF_UNSPEC */
	{
		struct sockaddr_in la, d = { .sin_family = AF_INET, .sin_port = htons(9) };
		u1 = socket(AF_INET, SOCK_DGRAM, 0);
		d.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		CHECK(connect(u1, (struct sockaddr *)&d, sizeof d) == 0);
		sl = sizeof la;
		CHECK(getsockname(u1, (struct sockaddr *)&la, &sl) == 0 && la.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
		p = la.sin_port;
		d.sin_addr.s_addr = htonl(0xc0000201);
		if (connect(u1, (struct sockaddr *)&d, sizeof d) == 0) {
			sl = sizeof la;
			CHECK(getsockname(u1, (struct sockaddr *)&la, &sl) == 0 &&
			      la.sin_addr.s_addr != htonl(INADDR_LOOPBACK) && la.sin_port == p);
		}
		d.sin_family = AF_UNSPEC;
		CHECK(connect(u1, (struct sockaddr *)&d, sizeof d) == 0);
		sl = sizeof la;
		/* (the port: kept here, 0 on Linux) */
		CHECK(getsockname(u1, (struct sockaddr *)&la, &sl) == 0 && la.sin_addr.s_addr == htonl(INADDR_ANY) &&
		      (la.sin_port == p || la.sin_port == 0));
		close(u1);
	}
	/* UDP over IPv6 */
	u1 = socket(AF_INET6, SOCK_DGRAM, 0);
	u2 = socket(AF_INET6, SOCK_DGRAM, 0);
	s6 = v6("::1", 0);
	CHECK(bind(u1, (struct sockaddr *)&s6, sizeof s6) == 0);
	s6.sin6_port = htons((unsigned short)port_of(u1));
	CHECK(sendto(u2, "six", 3, 0, (struct sockaddr *)&s6, sizeof s6) == 3);
	sl = sizeof s6;
	CHECK(recvfrom(u1, buf, sizeof buf, 0, (struct sockaddr *)&s6, &sl) == 3 && IN6_IS_ADDR_LOOPBACK(&s6.sin6_addr));
	close(u1);
	close(u2);

	/* raw sockets: ping 127.0.0.1 and ::1; a protocol of one's own */
	{
		int r4 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP), got = 0;
		unsigned char pk[64], in[128];
		CHECK(r4 >= 0);
		memset(pk, 0, sizeof pk);
		pk[0] = 8;                          /* echo request */
		pk[4] = 0x12, pk[5] = 0x34, pk[7] = 1;
		memcpy(pk + 8, "payload", 7);
		{
			unsigned sum = 0;
			for (int i = 0; i < 16; i += 2)
				sum += (unsigned)(pk[i] << 8 | pk[i + 1]);
			while (sum >> 16)
				sum = (sum & 0xffff) + (sum >> 16);
			sum = ~sum & 0xffff;
			pk[2] = (unsigned char)(sum >> 8), pk[3] = (unsigned char)sum;
		}
		sa = v4("127.0.0.1", 0);
		CHECK(sendto(r4, pk, 16, 0, (struct sockaddr *)&sa, sizeof sa) == 16);
		for (int i = 0; i < 4 && !got; i++) {
			ssize_t n = recv(r4, in, sizeof in, MSG_DONTWAIT);
			if (n == 36 && in[0] == 0x45 && in[9] == 1 && in[20] == 0 && in[24] == 0x12 && in[25] == 0x34 &&
			    !memcmp(in + 28, "payload", 7))
				got = 1;
		}
		CHECK(got);
		close(r4);

		int r6 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
		CHECK(r6 >= 0);
		memset(pk, 0, sizeof pk);
		pk[0] = 128;                        /* echo request; the kernel sums it */
		pk[4] = 0x56, pk[7] = 2;
		s6 = v6("::1", 0);
		CHECK(sendto(r6, pk, 12, 0, (struct sockaddr *)&s6, sizeof s6) == 12);
		got = 0;
		for (int i = 0; i < 4 && !got; i++) {
			sl = sizeof s6;
			ssize_t n = recvfrom(r6, in, sizeof in, MSG_DONTWAIT, (struct sockaddr *)&s6, &sl);
			if (n == 12 && in[0] == 129 && in[4] == 0x56 && IN6_IS_ADDR_LOOPBACK(&s6.sin6_addr))
				got = 1;
		}
		CHECK(got);
		close(r6);

		int ra = socket(AF_INET, SOCK_RAW, 253), rb = socket(AF_INET, SOCK_RAW, 253);
		int sock_type;
		sl = sizeof sock_type;
		CHECK(getsockopt(ra, SOL_SOCKET, SO_TYPE, &sock_type, &sl) == 0 && sock_type == SOCK_RAW);
		CHECK(sendto(ra, "raw!", 4, 0, (struct sockaddr *)&sa, sizeof sa) == 4);
		CHECK(recv(rb, in, sizeof in, MSG_DONTWAIT) == 24 && in[9] == 253 && !memcmp(in + 20, "raw!", 4));
		close(ra);
		close(rb);
	}

	/* the interface and the name service */
	CHECK(if_nametoindex("lo0") == 1);
	CHECK(if_indextoname(1, txt) && !strcmp(txt, "lo0"));
	{
		struct if_nameindex *ni = if_nameindex();
		CHECK(ni && ni[0].if_index == 1 && !strcmp(ni[0].if_name, "lo0") &&
		      (ni[1].if_index == 0 ||
		       (ni[1].if_index == 2 && (!strcmp(ni[1].if_name, "em0") || !strcmp(ni[1].if_name, "vio0")) &&
			ni[2].if_index == 0)));
		if (ni)
			if_freenameindex(ni);
	}
	{
		struct addrinfo h, *r;
		memset(&h, 0, sizeof h);
		h.ai_socktype = SOCK_STREAM;
		CHECK(getaddrinfo("localhost", "7", &h, &r) == 0);
		if (r)
			freeaddrinfo(r);
	}

	printf("k_inet: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
