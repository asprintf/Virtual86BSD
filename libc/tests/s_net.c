/* s_net.c - sockets and the resolver: Unix-domain stream and datagram
 * sockets, descriptor passing (with the close-on-exec and emulated
 * close-on-fork flags), TCP and UDP over the loopback, socket options,
 * cancellation in accept(); and the DNS resolver against a small DNS
 * server of the test's own on 127.0.0.1:53 (if the test may bind it),
 * through the resolver's configuration paths (__resolv_conf_path,
 * __hosts_path: internals of this library). */
#define _XOPEN_SOURCE 800
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: line %d: %s\n", __LINE__, #c); fflush(stdout); fails++; } } while (0)

extern const char *__resolv_conf_path, *__hosts_path;

static void unix_sockets(void)
{
	int sv[2], p[2], fd, r, flags;
	char buf[64];
	struct msghdr m;
	struct iovec iov;
	union { struct cmsghdr c; char b[CMSG_SPACE(sizeof(int)) + 16]; } ctl;
	struct cmsghdr *c;
	struct sockaddr_un sa;
	socklen_t sl;
	char path[64];

	CHECK(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0);
	CHECK(fcntl(sv[0], F_GETFD) & FD_CLOEXEC);
	CHECK(send(sv[0], "hello world", 11, 0) == 11);
	CHECK(recv(sv[1], buf, 5, MSG_PEEK) == 5 && !memcmp(buf, "hello", 5));
	CHECK(recv(sv[1], buf, 11, MSG_WAITALL) == 11 && !memcmp(buf, "hello world", 11));
	CHECK(recv(sv[1], buf, 1, MSG_DONTWAIT) == -1 && errno == EAGAIN);

	/* pass a pipe's descriptor; padding of msghdr and cmsghdr garbage */
	pipe(p);
	memset(&m, 0xff, sizeof m);
	memset(&ctl, 0xff, sizeof ctl);
	iov.iov_base = "x";
	iov.iov_len = 1;
	m.msg_name = 0;
	m.msg_namelen = 0;
	m.msg_iov = &iov;
	m.msg_iovlen = 1;
	m.msg_control = ctl.b;
	m.msg_controllen = CMSG_SPACE(sizeof(int));
	m.msg_flags = 0;
	c = CMSG_FIRSTHDR(&m);
	c->cmsg_level = SOL_SOCKET;
	c->cmsg_type = SCM_RIGHTS;
	c->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(c), &p[1], sizeof(int));
	CHECK(CMSG_NXTHDR(&m, c) == 0);
	CHECK(sendmsg(sv[0], &m, 0) == 1);
	memset(&m, 0xff, sizeof m);
	memset(&ctl, 0, sizeof ctl);
	iov.iov_base = buf;
	iov.iov_len = sizeof buf;
	m.msg_name = 0;
	m.msg_namelen = 0;
	m.msg_iov = &iov;
	m.msg_iovlen = 1;
	m.msg_control = ctl.b;
	m.msg_controllen = sizeof ctl.b;
	m.msg_flags = 0;
	CHECK(recvmsg(sv[1], &m, MSG_CMSG_CLOEXEC | MSG_CMSG_CLOFORK) == 1 && buf[0] == 'x');
	c = CMSG_FIRSTHDR(&m);
	CHECK(c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS && c->cmsg_len == CMSG_LEN(sizeof(int)));
	CHECK(!(m.msg_flags & MSG_CTRUNC));
	if (c) {
		memcpy(&fd, CMSG_DATA(c), sizeof fd);
		flags = fcntl(fd, F_GETFD);
		CHECK(flags & FD_CLOEXEC);
		CHECK(flags & FD_CLOFORK);
		CHECK(write(fd, "y", 1) == 1 && read(p[0], buf, 1) == 1 && buf[0] == 'y');
		close(fd);
	}
	close(p[0]);
	close(p[1]);
	close(sv[0]);
	close(sv[1]);

	/* SOCK_CLOFORK: not in the child */
	fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOFORK, 0);
	CHECK(fd >= 0 && (fcntl(fd, F_GETFD) & FD_CLOFORK));
	{
		pid_t pid = fork();
		int st;
		if (!pid)
			_exit(fcntl(fd, F_GETFD) == -1 && errno == EBADF ? 0 : 1);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}
	close(fd);
	CHECK(accept4(0, 0, 0, 1) == -1 && errno == EINVAL);

	/* datagrams on a named socket */
	snprintf(path, sizeof path, "/tmp/s_net.%d", (int)getpid());
	unlink(path);
	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	strcpy(sa.sun_path, path);
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	CHECK(bind(fd, (struct sockaddr *)&sa, sizeof sa) == 0);
	r = socket(AF_UNIX, SOCK_DGRAM, 0);
	CHECK(sendto(r, "dgram", 5, 0, (struct sockaddr *)&sa, sizeof sa) == 5);
	CHECK(recv(fd, buf, sizeof buf, 0) == 5 && !memcmp(buf, "dgram", 5));
	sl = sizeof sa;
	CHECK(getsockname(fd, (struct sockaddr *)&sa, &sl) == 0 && !strcmp(sa.sun_path, path));
	close(r);
	close(fd);
	unlink(path);
}

static void *accepter(void *p)
{
	accept(*(int *)p, 0, 0);
	return 0;
}

static void inet_sockets(void)
{
	struct sockaddr_in sa, peer;
	socklen_t sl = sizeof sa;
	int l, c, a, v, u1, u2;
	char buf[64];
	struct addrinfo h, *res;
	char port[16];
	pthread_t t;
	void *r;

	l = socket(AF_INET, SOCK_STREAM, 0);
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	v = 1;
	CHECK(setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v) == 0);
	CHECK(bind(l, (struct sockaddr *)&sa, sizeof sa) == 0);
	CHECK(getsockname(l, (struct sockaddr *)&sa, &sl) == 0 && sa.sin_port != 0);
	CHECK(listen(l, 5) == 0);
	sl = sizeof v;
	CHECK(getsockopt(l, SOL_SOCKET, SO_ACCEPTCONN, &v, &sl) == 0 && v == 1);
	CHECK(getsockopt(l, SOL_SOCKET, SO_TYPE, &v, &sl) == 0 && v == SOCK_STREAM);
	CHECK(getsockopt(l, SOL_SOCKET, SO_DOMAIN, &v, &sl) == 0 && v == AF_INET);
	CHECK(getsockopt(l, SOL_SOCKET, SO_PROTOCOL, &v, &sl) == 0 && v == IPPROTO_TCP);

	/* connect through getaddrinfo */
	memset(&h, 0, sizeof h);
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;
	snprintf(port, sizeof port, "%d", ntohs(sa.sin_port));
	CHECK(getaddrinfo("localhost", port, &h, &res) == 0);
	c = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	CHECK(connect(c, res->ai_addr, res->ai_addrlen) == 0);
	freeaddrinfo(res);
	sl = sizeof peer;
	a = accept4(l, (struct sockaddr *)&peer, &sl, SOCK_CLOEXEC | SOCK_NONBLOCK);
	CHECK(a >= 0 && (fcntl(a, F_GETFD) & FD_CLOEXEC) && (fcntl(a, F_GETFL) & O_NONBLOCK));
	CHECK(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
	v = 1;
	CHECK(setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v) == 0);
	CHECK(send(c, "ping", 4, MSG_NOSIGNAL) == 4);
	{
		struct pollfd pf = {a, POLLIN, 0};
		poll(&pf, 1, 1000);
	}
	CHECK(recv(a, buf, sizeof buf, 0) == 4 && !memcmp(buf, "ping", 4));
	CHECK(sockatmark(a) == 0);
	CHECK(shutdown(c, SHUT_WR) == 0);
	{
		struct pollfd pf = {a, POLLIN, 0};
		poll(&pf, 1, 1000);
	}
	CHECK(recv(a, buf, sizeof buf, 0) == 0);
	sl = sizeof v;
	CHECK(getsockopt(c, SOL_SOCKET, SO_ERROR, &v, &sl) == 0 && v == 0);
	{
		struct linger lg = {1, 0};
		CHECK(setsockopt(c, SOL_SOCKET, SO_LINGER, &lg, sizeof lg) == 0);
	}
	close(a);
	close(c);

	/* accept() is a cancellation point */
	pthread_create(&t, 0, accepter, &l);
	{
		struct timespec ts = {0, 30000000};
		nanosleep(&ts, 0);
	}
	pthread_cancel(t);
	CHECK(pthread_join(t, &r) == 0 && r == PTHREAD_CANCELED);
	close(l);

	/* UDP */
	u1 = socket(AF_INET, SOCK_DGRAM, 0);
	u2 = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bind(u1, (struct sockaddr *)&sa, sizeof sa);
	sl = sizeof sa;
	getsockname(u1, (struct sockaddr *)&sa, &sl);
	CHECK(sendto(u2, "udp!", 4, 0, (struct sockaddr *)&sa, sizeof sa) == 4);
	sl = sizeof peer;
	CHECK(recvfrom(u1, buf, sizeof buf, 0, (struct sockaddr *)&peer, &sl) == 4 && sl == sizeof peer &&
	      peer.sin_family == AF_INET && !memcmp(buf, "udp!", 4));
	{
		struct msghdr m;
		struct iovec iov[2] = {{"ab", 2}, {"cd", 2}};
		memset(&m, 0, sizeof m);
		m.msg_name = &sa;
		m.msg_namelen = sizeof sa;
		m.msg_iov = iov;
		m.msg_iovlen = 2;
		CHECK(sendmsg(u2, &m, 0) == 4);
		CHECK(recv(u1, buf, 1, 0) == 1 && buf[0] == 'a');   /* the rest is dropped */
	}
	close(u1);
	close(u2);

	/* IPv6, where the loopback has it */
	{
		int s6 = socket(AF_INET6, SOCK_STREAM, 0);
		struct sockaddr_in6 a6;
		memset(&a6, 0, sizeof a6);
		a6.sin6_family = AF_INET6;
		a6.sin6_addr = in6addr_loopback;
		if (s6 >= 0 && bind(s6, (struct sockaddr *)&a6, sizeof a6) == 0) {
			sl = sizeof a6;
			CHECK(getsockname(s6, (struct sockaddr *)&a6, &sl) == 0 && IN6_IS_ADDR_LOOPBACK(&a6.sin6_addr));
		}
		if (s6 >= 0)
			close(s6);
	}
}

/* ---- a DNS server for the resolver ---- */
static int dns_udp = -1, dns_tcp = -1;
static volatile int dns_stop, dns_queries;

static int put_name(unsigned char *p, const char *name)
{
	int n = 0;
	while (*name) {
		const char *dot = strchr(name, '.');
		int l = dot ? (int)(dot - name) : (int)strlen(name);
		p[n++] = (unsigned char)l;
		memcpy(p + n, name, (size_t)l);
		n += l;
		name += l;
		if (*name)
			name++;
	}
	p[n++] = 0;
	return n;
}

static int put_rr(unsigned char *p, int type, const void *data, int len)
{
	p[0] = 0xc0; p[1] = 12;                      /* the question's name */
	p[2] = 0; p[3] = (unsigned char)type; p[4] = 0; p[5] = 1;
	p[6] = 0; p[7] = 0; p[8] = 0x0e; p[9] = 0x10;  /* TTL */
	p[10] = (unsigned char)(len >> 8); p[11] = (unsigned char)len;
	memcpy(p + 12, data, (size_t)len);
	return 12 + len;
}

/* an answer to the query q: its length, or 0 for none */
static int answer(const unsigned char *q, int ql, unsigned char *a, int tcp)
{
	char name[256];
	int i = 12, o = 0, type, n, an = 0, rcode = 0;
	unsigned char rd[256];
	while (q[i] && i < ql) {
		if (o)
			name[o++] = '.';
		memcpy(name + o, q + i + 1, q[i]);
		o += q[i];
		i += q[i] + 1;
	}
	name[o] = 0;
	i++;
	type = q[i + 1];
	i += 4;
	memcpy(a, q, (size_t)i);
	a[2] = 0x81;
	a[3] = 0x80;
	n = i;
	dns_queries++;
	if (!strcmp(name, "slow.example.test"))
		return 0;
	if (!strcmp(name, "a.example.test") || !strcmp(name, "short.example.test")) {
		if (type == 1) {
			unsigned char a1[4] = {10, 0, 0, 1}, a2[4] = {10, 0, 0, 2};
			if (name[0] == 's') a1[1] = a1[2] = a1[3] = 9;
			n += put_rr(a + n, 1, a1, 4); an++;
			if (name[0] == 'a') { n += put_rr(a + n, 1, a2, 4); an++; }
		} else if (type == 28 && name[0] == 'a') {
			unsigned char a6[16] = {0x20, 0x01, 0x0d, 0xb8};
			a6[15] = 1;
			n += put_rr(a + n, 28, a6, 16); an++;
		}
	} else if (!strcmp(name, "alias.example.test")) {
		unsigned char a1[4] = {10, 0, 0, 1};
		int l = put_name(rd, "a.example.test");
		n += put_rr(a + n, 5, rd, l); an++;
		if (type == 1) {
			n += put_rr(a + n, 1, a1, 4); an++;
		}
	} else if (!strcmp(name, "big.example.test")) {
		if (!tcp) {
			a[2] |= 2;           /* truncated */
		} else if (type == 1) {
			for (an = 0; an < 30; an++) {
				unsigned char a1[4] = {10, 1, 0, (unsigned char)an};
				n += put_rr(a + n, 1, a1, 4);
			}
		}
	} else if (!strcmp(name, "1.0.0.10.in-addr.arpa") && type == 12) {
		int l = put_name(rd, "a.example.test");
		n += put_rr(a + n, 12, rd, l); an++;
	} else
		rcode = 3;
	a[3] |= (unsigned char)rcode;
	a[6] = (unsigned char)(an >> 8);
	a[7] = (unsigned char)an;
	return n;
}

static void *dns_server(void *p)
{
	unsigned char q[512], a[4096];
	struct pollfd pf[2] = {{dns_udp, POLLIN, 0}, {dns_tcp, POLLIN, 0}};
	(void)p;
	while (!dns_stop) {
		if (poll(pf, 2, 100) <= 0)
			continue;
		if (pf[0].revents & POLLIN) {
			struct sockaddr_in from;
			socklen_t fl = sizeof from;
			int n = (int)recvfrom(dns_udp, q, sizeof q, 0, (struct sockaddr *)&from, &fl), an;
			if (n > 12 && (an = answer(q, n, a, 0)))
				sendto(dns_udp, a, (size_t)an, 0, (struct sockaddr *)&from, fl);
		}
		if (pf[1].revents & POLLIN) {
			int c = accept(dns_tcp, 0, 0), n, an;
			unsigned char len[2];
			if (c < 0)
				continue;
			if (recv(c, len, 2, MSG_WAITALL) == 2) {
				n = len[0] << 8 | len[1];
				if (recv(c, q, (size_t)n, MSG_WAITALL) == n && (an = answer(q, n, a, 1))) {
					len[0] = (unsigned char)(an >> 8);
					len[1] = (unsigned char)an;
					send(c, len, 2, 0);
					send(c, a, (size_t)an, 0);
				}
			}
			close(c);
		}
	}
	return 0;
}

static int count(struct addrinfo *r)
{
	int n = 0;
	for (; r; r = r->ai_next)
		n++;
	return n;
}

static const char *addr_of(struct addrinfo *r, char *b)
{
	void *a = r->ai_family == AF_INET ? (void *)&((struct sockaddr_in *)r->ai_addr)->sin_addr
	                                  : (void *)&((struct sockaddr_in6 *)r->ai_addr)->sin6_addr;
	return inet_ntop(r->ai_family, a, b, 64);
}

static void resolver(void)
{
	struct sockaddr_in sa;
	char conf[64], hosts[64], b[64], host[256];
	struct addrinfo h, *r;
	pthread_t t;
	FILE *f;
	int v = 1, e;
	struct timespec t0, t1;

	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(53);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	dns_udp = socket(AF_INET, SOCK_DGRAM, 0);
	dns_tcp = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(dns_tcp, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
	if (bind(dns_udp, (struct sockaddr *)&sa, sizeof sa) < 0 || bind(dns_tcp, (struct sockaddr *)&sa, sizeof sa) < 0 ||
	    listen(dns_tcp, 5) < 0) {
		printf("resolver: cannot serve on 127.0.0.1:53 (%s): not tested\n", strerror(errno));
		close(dns_udp);
		close(dns_tcp);
		return;
	}
	snprintf(conf, sizeof conf, "/tmp/s_net.resolv.%d", (int)getpid());
	snprintf(hosts, sizeof hosts, "/tmp/s_net.hosts.%d", (int)getpid());
	f = fopen(conf, "w");
	fprintf(f, "# test\nnameserver 127.0.0.1\nsearch example.test\noptions timeout:1 attempts:2 ndots:1\n");
	fclose(f);
	f = fopen(hosts, "w");
	fprintf(f, "192.0.2.7 fromfile.example.test fromfile\n");
	fclose(f);
	__resolv_conf_path = conf;
	__hosts_path = hosts;
	pthread_create(&t, 0, dns_server, 0);

	memset(&h, 0, sizeof h);
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;
	h.ai_flags = AI_CANONNAME;
	CHECK((e = getaddrinfo("a.example.test", "80", &h, &r)) == 0);
	if (!e) {
		CHECK(count(r) == 2 && !strcmp(addr_of(r, b), "10.0.0.1") && !strcmp(addr_of(r->ai_next, b), "10.0.0.2"));
		CHECK(r->ai_canonname && !strcmp(r->ai_canonname, "a.example.test"));
		freeaddrinfo(r);
	}
	CHECK((e = getaddrinfo("alias.example.test", 0, &h, &r)) == 0);
	if (!e) {
		CHECK(count(r) == 1 && !strcmp(addr_of(r, b), "10.0.0.1"));
		CHECK(r->ai_canonname && !strcmp(r->ai_canonname, "a.example.test"));
		freeaddrinfo(r);
	}
	CHECK((e = getaddrinfo("big.example.test", 0, &h, &r)) == 0);     /* over TCP */
	if (!e) {
		CHECK(count(r) == 30);
		freeaddrinfo(r);
	}
	CHECK((e = getaddrinfo("short", 0, &h, &r)) == 0);                 /* the search list */
	if (!e) {
		CHECK(count(r) == 1 && !strcmp(addr_of(r, b), "10.9.9.9"));
		freeaddrinfo(r);
	}
	CHECK((e = getaddrinfo("fromfile", 0, &h, &r)) == 0);             /* the hosts file first */
	if (!e) {
		CHECK(count(r) == 1 && !strcmp(addr_of(r, b), "192.0.2.7") && !strcmp(r->ai_canonname, "fromfile.example.test"));
		freeaddrinfo(r);
	}
	CHECK(getaddrinfo("nx.example.test", 0, &h, &r) == EAI_NONAME);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	CHECK(getaddrinfo("slow.example.test.", 0, &h, &r) == EAI_AGAIN);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	CHECK(t1.tv_sec - t0.tv_sec >= 1 && t1.tv_sec - t0.tv_sec <= 4);
	h.ai_family = AF_UNSPEC;
	h.ai_flags = 0;
	CHECK((e = getaddrinfo("a.example.test", 0, &h, &r)) == 0);
	if (!e) {
		CHECK(count(r) == 3 && r->ai_family == AF_INET);   /* no IPv6 route: last */
		freeaddrinfo(r);
	}
	h.ai_family = AF_INET6;
	CHECK((e = getaddrinfo("a.example.test", 0, &h, &r)) == 0);
	if (!e) {
		CHECK(count(r) == 1 && !strcmp(addr_of(r, b), "2001:db8::1"));
		freeaddrinfo(r);
	}
	h.ai_flags = AI_V4MAPPED | AI_ALL;
	CHECK((e = getaddrinfo("a.example.test", 0, &h, &r)) == 0);
	if (!e) {
		CHECK(count(r) == 3);
		freeaddrinfo(r);
	}

	sa.sin_addr.s_addr = htonl(0x0a000001);
	sa.sin_port = htons(22);
	CHECK(getnameinfo((struct sockaddr *)&sa, sizeof sa, host, sizeof host, b, 32, 0) == 0 &&
	      !strcmp(host, "a.example.test") && !strcmp(b, "ssh"));
	CHECK(getnameinfo((struct sockaddr *)&sa, sizeof sa, host, sizeof host, 0, 0, NI_NOFQDN) == 0 && !strcmp(host, "a"));
	sa.sin_addr.s_addr = htonl(0x0a000002);
	CHECK(getnameinfo((struct sockaddr *)&sa, sizeof sa, host, sizeof host, 0, 0, NI_NAMEREQD) == EAI_NONAME);
	CHECK(getnameinfo((struct sockaddr *)&sa, sizeof sa, host, sizeof host, 0, 0, 0) == 0 && !strcmp(host, "10.0.0.2"));
	sa.sin_addr.s_addr = htonl(0xc0000207);
	CHECK(getnameinfo((struct sockaddr *)&sa, sizeof sa, host, sizeof host, 0, 0, 0) == 0 &&
	      !strcmp(host, "fromfile.example.test"));
	CHECK(dns_queries > 8);

	dns_stop = 1;
	pthread_join(t, 0);
	close(dns_udp);
	close(dns_tcp);
	unlink(conf);
	unlink(hosts);
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	unix_sockets();
	inet_sockets();
	resolver();
	printf("%d failures\n", fails);
	return fails != 0;
}
