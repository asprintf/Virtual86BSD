/* A DNS stub resolver: A, AAAA and PTR queries to the name servers of
 * /etc/resolv.conf ("nameserver", "search"/"domain", "options ndots:
 * timeout: attempts:"), over UDP to all servers at once, retried every
 * `timeout' seconds `attempts' times, and over TCP when an answer comes
 * back truncated.  Names with fewer than ndots dots go through the
 * search list first. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "netdb_impl.h"

const char *__resolv_conf_path = "/etc/resolv.conf";

#define T_A     1
#define T_CNAME 5
#define T_PTR   12
#define T_AAAA  28
#define MAXNS 3
#define ABUF 4096

struct conf {
	struct sockaddr_in6 ns[MAXNS];
	int nns, ndots, timeout, attempts;
	char search[256];
};

/* a numeric address as a socket address (IPv4 ones v4-mapped) */
static int ns_addr(struct sockaddr_in6 *sa, const char *s)
{
	unsigned char a4[4];
	char buf[64], *pct;
	memset(sa, 0, sizeof *sa);
	sa->sin6_family = AF_INET6;
	sa->sin6_port = htons(53);
	if (inet_pton(AF_INET, s, a4) == 1) {
		sa->sin6_addr.s6_addr[10] = sa->sin6_addr.s6_addr[11] = 0xff;
		memcpy(sa->sin6_addr.s6_addr + 12, a4, 4);
		return 1;
	}
	if (strlen(s) >= sizeof buf)
		return 0;
	strcpy(buf, s);
	if ((pct = strchr(buf, '%')))
		*pct = 0;
	return inet_pton(AF_INET6, buf, &sa->sin6_addr) == 1;
}

static void get_conf(struct conf *c)
{
	FILE *f = fopen(__resolv_conf_path, "re");
	char line[512], *fl[8];
	int n, i;
	memset(c, 0, sizeof *c);
	c->ndots = 1;
	c->timeout = 5;
	c->attempts = 2;
	while (f && fgets(line, sizeof line, f)) {
		if (line[0] == ';')
			continue;
		n = __netdb_fields(line, fl, 8);
		if (n < 2)
			continue;
		if (!strcmp(fl[0], "nameserver") && c->nns < MAXNS) {
			if (ns_addr(&c->ns[c->nns], fl[1]))
				c->nns++;
		} else if (!strcmp(fl[0], "options")) {
			for (i = 1; i < n; i++) {
				if (!strncmp(fl[i], "ndots:", 6)) c->ndots = atoi(fl[i] + 6);
				else if (!strncmp(fl[i], "timeout:", 8)) c->timeout = atoi(fl[i] + 8);
				else if (!strncmp(fl[i], "attempts:", 9)) c->attempts = atoi(fl[i] + 9);
			}
		} else if (!strcmp(fl[0], "search") || !strcmp(fl[0], "domain")) {
			c->search[0] = 0;
			for (i = 1; i < n; i++)
				if (strlen(c->search) + strlen(fl[i]) + 2 < sizeof c->search) {
					strcat(c->search, fl[i]);
					strcat(c->search, " ");
				}
		}
	}
	if (f)
		fclose(f);
	if (c->ndots > 15) c->ndots = 15;
	if (c->timeout < 1) c->timeout = 1;
	if (c->timeout > 30) c->timeout = 30;
	if (c->attempts < 1) c->attempts = 1;
	if (c->attempts > 5) c->attempts = 5;
	if (!c->nns) {
		ns_addr(&c->ns[0], "127.0.0.1");
		c->nns = 1;
	}
}

static unsigned short new_id(void)
{
	static unsigned seq;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (unsigned short)((unsigned)ts.tv_nsec ^ (unsigned)ts.tv_nsec >> 16 ^ ++seq * 40503u);
}

/* a query in q: its length, or -1 (a bad name) */
static int mkquery(unsigned char *q, const char *name, int type)
{
	unsigned short id = new_id();
	int i = 12, len;
	size_t n = strlen(name);
	if (n && name[n - 1] == '.')
		n--;
	if (n > 253)
		return -1;
	memset(q, 0, 12);
	q[0] = (unsigned char)(id >> 8);
	q[1] = (unsigned char)id;
	q[2] = 1;                      /* RD */
	q[5] = 1;                      /* one question */
	while (n) {
		const char *dot = memchr(name, '.', n);
		len = dot ? (int)(dot - name) : (int)n;
		if (len < 1 || len > 63)
			return -1;
		q[i++] = (unsigned char)len;
		memcpy(q + i, name, len);
		i += len;
		name += len;
		n -= len;
		if (n) {
			name++;
			n--;
		}
	}
	q[i++] = 0;
	q[i++] = 0; q[i++] = (unsigned char)type;
	q[i++] = 0; q[i++] = 1;        /* IN */
	return i;
}

/* a name at p in the message: its text in out, the next offset; -1 */
static int getname(const unsigned char *m, int len, int p, char *out, int outlen)
{
	int jumps = 0, o = 0, end = -1;
	for (;;) {
		int l;
		if (p >= len)
			return -1;
		l = m[p];
		if ((l & 0xc0) == 0xc0) {
			if (p + 1 >= len || ++jumps > 64)
				return -1;
			if (end < 0)
				end = p + 2;
			p = (l & 0x3f) << 8 | m[p + 1];
			continue;
		}
		if (l & 0xc0)
			return -1;
		if (!l) {
			if (end < 0)
				end = p + 1;
			break;
		}
		if (p + 1 + l > len || o + l + 2 > outlen)
			return -1;
		if (o)
			out[o++] = '.';
		memcpy(out + o, m + p + 1, l);
		o += l;
		p += 1 + l;
	}
	out[o] = 0;
	return end;
}

struct rr { int type, rdlen, rdata; char name[256]; };

/* the answer records, through f; the rcode, or -1 for a bad message */
static int walk(const unsigned char *m, int len, int (*f)(void *, const unsigned char *, int, struct rr *), void *ctx)
{
	int p = 12, qd, an, i;
	struct rr r;
	char tmp[256];
	if (len < 12)
		return -1;
	qd = m[4] << 8 | m[5];
	an = m[6] << 8 | m[7];
	for (i = 0; i < qd; i++) {
		if ((p = getname(m, len, p, tmp, sizeof tmp)) < 0 || (p += 4) > len)
			return -1;
	}
	for (i = 0; i < an; i++) {
		if ((p = getname(m, len, p, r.name, sizeof r.name)) < 0 || p + 10 > len)
			return -1;
		r.type = m[p] << 8 | m[p + 1];
		r.rdlen = m[p + 8] << 8 | m[p + 9];
		r.rdata = p + 10;
		p += 10 + r.rdlen;
		if (p > len)
			return -1;
		if (f(ctx, m, len, &r))
			break;
	}
	return m[3] & 15;
}

static int same_ns(const struct sockaddr_in6 *a, const struct sockaddr_in6 *b)
{
	return a->sin6_port == b->sin6_port && !memcmp(&a->sin6_addr, &b->sin6_addr, 16);
}

/* over TCP: the answer's length, or -1 */
static int tcp_query(const struct sockaddr_in6 *ns, int v4only, const unsigned char *q, int ql,
                     unsigned char *a, int timeout)
{
	unsigned char lenb[2];
	struct pollfd pfd;
	int fd, n = 0, need = 2, got = 0, r;
	if (v4only) {
		struct sockaddr_in s4 = {AF_INET, ns->sin6_port, {0}, {0}};
		memcpy(&s4.sin_addr, ns->sin6_addr.s6_addr + 12, 4);
		fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (fd < 0 || connect(fd, (struct sockaddr *)&s4, sizeof s4) < 0)
			goto fail;
	} else {
		fd = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (fd < 0 || connect(fd, (const struct sockaddr *)ns, sizeof *ns) < 0)
			goto fail;
	}
	lenb[0] = (unsigned char)(ql >> 8);
	lenb[1] = (unsigned char)ql;
	if (send(fd, lenb, 2, MSG_NOSIGNAL) != 2 || send(fd, q, (size_t)ql, MSG_NOSIGNAL) != ql)
		goto fail;
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (got < need) {
		if (poll(&pfd, 1, timeout * 1000) <= 0)
			goto fail;
		if (got < 2)
			r = (int)recv(fd, lenb + got, 2 - (size_t)got, 0);
		else
			r = (int)recv(fd, a + got - 2, (size_t)(need - got), 0);
		if (r <= 0)
			goto fail;
		got += r;
		if (got == 2) {
			n = lenb[0] << 8 | lenb[1];
			if (n > ABUF)
				n = ABUF;
			need = 2 + n;
		}
	}
	close(fd);
	return n;
fail:
	if (fd >= 0)
		close(fd);
	return -1;
}

/* send the nq queries, wait for their answers: alen[i] 0 for none;
 * 0, or -1 when no server could be reached at all */
static int run(const struct conf *c, unsigned char (*q)[300], const int *ql, int nq,
               unsigned char (*a)[ABUF], int *alen)
{
	struct sockaddr_in6 from;
	struct sockaddr_in s4;
	socklen_t fl;
	struct pollfd pfd;
	int fd, v4only = 0, i, j, k, r, left = nq, sent, attempt;
	long t0, now;
	struct timespec ts;

	for (i = 0; i < nq; i++)
		alen[i] = 0;
	fd = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd >= 0) {
		int off = 0;
		setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off);
	} else {
		fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
		v4only = 1;
		if (fd < 0)
			return -1;
	}
	pfd.fd = fd;
	pfd.events = POLLIN;
	for (attempt = 0; attempt < c->attempts && left; attempt++) {
		sent = 0;
		for (i = 0; i < nq; i++) {
			if (alen[i])
				continue;
			for (j = 0; j < c->nns; j++) {
				if (v4only) {
					if (!IN6_IS_ADDR_V4MAPPED(&c->ns[j].sin6_addr))
						continue;
					memset(&s4, 0, sizeof s4);
					s4.sin_family = AF_INET;
					s4.sin_port = c->ns[j].sin6_port;
					memcpy(&s4.sin_addr, c->ns[j].sin6_addr.s6_addr + 12, 4);
					r = (int)sendto(fd, q[i], (size_t)ql[i], MSG_NOSIGNAL, (struct sockaddr *)&s4, sizeof s4);
				} else
					r = (int)sendto(fd, q[i], (size_t)ql[i], MSG_NOSIGNAL,
					                (const struct sockaddr *)&c->ns[j], sizeof c->ns[j]);
				if (r >= 0)
					sent++;
			}
		}
		if (!sent) {
			close(fd);
			return -1;
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		t0 = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		for (;;) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			now = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
			if (!left || now - t0 >= c->timeout * 1000L)
				break;
			if (poll(&pfd, 1, (int)(c->timeout * 1000L - (now - t0))) <= 0)
				continue;
			for (;;) {
				unsigned char buf[ABUF];
				memset(&from, 0, sizeof from);
				fl = sizeof from;
				r = (int)recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
				if (r < 0)
					break;
				if (r < 12)
					continue;
				if (v4only) {     /* as a mapped address, to compare */
					struct sockaddr_in *f4 = (struct sockaddr_in *)&from;
					struct sockaddr_in6 m;
					memset(&m, 0, sizeof m);
					m.sin6_port = f4->sin_port;
					m.sin6_addr.s6_addr[10] = m.sin6_addr.s6_addr[11] = 0xff;
					memcpy(m.sin6_addr.s6_addr + 12, &f4->sin_addr, 4);
					from = m;
				}
				for (j = 0; j < c->nns && !same_ns(&from, &c->ns[j]); j++)
					;
				if (j == c->nns)
					continue;
				for (i = 0; i < nq; i++)
					if (!alen[i] && buf[0] == q[i][0] && buf[1] == q[i][1] &&
					    r >= ql[i] && !memcmp(buf + 12, q[i] + 12, (size_t)ql[i] - 12))
						break;
				if (i == nq)
					continue;
				k = buf[3] & 15;
				if (k != 0 && k != 3)          /* SERVFAIL, REFUSED ...: another may answer */
					continue;
				if (buf[2] & 2) {              /* truncated: again over TCP */
					int n = tcp_query(&c->ns[j], v4only, q[i], ql[i], a[i], c->timeout);
					if (n < 12)
						continue;
					alen[i] = n;
				} else {
					memcpy(a[i], buf, (size_t)r);
					alen[i] = r;
				}
				left--;
			}
		}
	}
	close(fd);
	return 0;
}

struct collect {
	struct __address *buf;
	int max, n;
	char *canon;
	int found_name;
};

static int add_rr(void *p, const unsigned char *m, int len, struct rr *r)
{
	struct collect *c = p;
	if (r->type == T_CNAME) {
		char t[256];
		if (getname(m, len, r->rdata, t, sizeof t) > 0)
			strcpy(c->canon, t);
	} else if ((r->type == T_A && r->rdlen == 4) || (r->type == T_AAAA && r->rdlen == 16)) {
		if (c->n < c->max) {
			memset(&c->buf[c->n], 0, sizeof c->buf[c->n]);
			c->buf[c->n].family = r->type == T_A ? AF_INET : AF_INET6;
			memcpy(c->buf[c->n].addr, m + r->rdata, (size_t)r->rdlen);
			c->n++;
		}
	}
	return 0;
}

/* one fully qualified name: the number of addresses, 0 (none: NXDOMAIN
 * or no data), or EAI_AGAIN / EAI_FAIL */
static int lookup1(const struct conf *cf, struct __address *buf, int max, char canon[256], const char *name, int family)
{
	unsigned char q[2][300], a[2][ABUF];
	int ql[2], alen[2], nq = 0, i, any = 0;
	struct collect c = {buf, max, 0, canon, 0};
	if (family != AF_INET6) {
		if ((ql[nq] = mkquery(q[nq], name, T_A)) < 0) return 0;
		nq++;
	}
	if (family != AF_INET) {
		if ((ql[nq] = mkquery(q[nq], name, T_AAAA)) < 0) return 0;
		nq++;
	}
	strcpy(canon, name);
	if (*canon && canon[strlen(canon) - 1] == '.')
		canon[strlen(canon) - 1] = 0;
	if (run(cf, q, ql, nq, a, alen) < 0)
		return EAI_AGAIN;
	for (i = 0; i < nq; i++) {
		if (!alen[i])
			continue;
		any = 1;
		if (walk(a[i], alen[i], add_rr, &c) < 0)
			return EAI_FAIL;
	}
	if (!any)
		return EAI_AGAIN;
	return c.n;
}

int __dns_lookup(struct __address *buf, int max, char canon[256], const char *name, int family)
{
	struct conf c;
	char fq[512], *s, *e;
	int dots = 0, r, again = 0;
	const char *p;
	size_t n = strlen(name);
	if (!n || n > 253)
		return EAI_NONAME;
	get_conf(&c);
	for (p = name; *p; p++)
		dots += *p == '.';
	/* absolute, or enough dots: as is first */
	if (name[n - 1] == '.' || dots >= c.ndots) {
		r = lookup1(&c, buf, max, canon, name, family);
		if (r > 0 || name[n - 1] == '.')
			return r > 0 ? r : r == 0 ? EAI_NONAME : r;
		again |= r == EAI_AGAIN;
	}
	for (s = c.search; *s; s = e + 1) {
		e = strchr(s, ' ');
		if (!e)
			break;
		if (n + (size_t)(e - s) + 2 > sizeof fq)
			continue;
		memcpy(fq, name, n);
		fq[n] = '.';
		memcpy(fq + n + 1, s, (size_t)(e - s));
		fq[n + 1 + (e - s)] = 0;
		r = lookup1(&c, buf, max, canon, fq, family);
		if (r > 0)
			return r;
		again |= r == EAI_AGAIN;
	}
	if (dots < c.ndots) {
		r = lookup1(&c, buf, max, canon, name, family);
		if (r > 0)
			return r;
		again |= r == EAI_AGAIN;
	}
	return again ? EAI_AGAIN : EAI_NONAME;
}

static int get_ptr(void *p, const unsigned char *m, int len, struct rr *r)
{
	char *out = p;
	if (r->type == T_PTR && getname(m, len, r->rdata, out, 256) > 0)
		return 1;
	return 0;
}

int __dns_reverse(const unsigned char *addr, int family, char *name, size_t n)
{
	struct conf c;
	unsigned char q[1][300], a[1][ABUF];
	char qn[80], res[256];
	int ql[1], alen[1], i, rc;
	static const char hex[] = "0123456789abcdef";
	if (family == AF_INET)
		sprintf(qn, "%d.%d.%d.%d.in-addr.arpa", addr[3], addr[2], addr[1], addr[0]);
	else {
		char *p = qn;
		for (i = 15; i >= 0; i--) {
			*p++ = hex[addr[i] & 15]; *p++ = '.';
			*p++ = hex[addr[i] >> 4]; *p++ = '.';
		}
		strcpy(p, "ip6.arpa");
	}
	get_conf(&c);
	ql[0] = mkquery(q[0], qn, T_PTR);
	if (run(&c, q, ql, 1, a, alen) < 0 || !alen[0])
		return EAI_AGAIN;
	res[0] = 0;
	rc = walk(a[0], alen[0], get_ptr, res);
	if (rc < 0)
		return EAI_FAIL;
	if (!res[0] || strlen(res) >= n)
		return 0;
	strcpy(name, res);
	return 1;
}
