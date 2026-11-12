/* nethard.c - the network hardening tests, the host's side.
 *
 * It plays a hostile peer on a second network card of a V86BSD guest: QEMU
 * connects the card to this program (-netdev socket,connect=127.0.0.1:PORT,
 * frames with a 4 byte length in front), the guest has 10.9.0.2 on it and
 * runs hardsrv (an echo server on port 7000, a control port 7002).  This
 * program is 10.9.0.1 and every other address of 10.9.0.0/24 (it answers
 * their ARP), speaks Ethernet, IP, ICMP, UDP and a little TCP by hand, and
 * sends what a well behaved peer would not: forged sources, source routes,
 * overlapping and enormous fragments, floods of SYNs, blind resets...  Each
 * check prints "ok" or "FAIL" and the exit status says whether all passed.
 *
 *   cc -o nethard nethard.c && ./nethard PORT       (then boot the guest)
 *
 * run_hard.sh does both.  Written for Linux hosts; no packet privileges are
 * needed, the frames only go over the one socket.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define HIP 0x0a090001U
#define GIP 0x0a090002U
#define BCAST 0x0a0900ffU
static const uint8_t HMAC[6] = {2, 0, 0, 0, 0, 1};
static const uint8_t GMAC[6] = {0x52, 0x54, 0, 0x12, 0x34, 0x99};
static const uint8_t BMAC[6] = {255, 255, 255, 255, 255, 255};

static int fd, pass, fail;

static uint64_t now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

static void check(int ok, const char *name)
{
	if (ok)
		pass++;
	else
		fail++;
	printf("%s %s\n", ok ? "ok" : "FAIL", name);
	fflush(stdout);
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void wr16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v; }

static uint16_t csum(const void *b, size_t n, uint32_t sum)
{
	const uint8_t *p = b;

	for (; n > 1; p += 2, n -= 2)
		sum += (uint32_t)(p[0] << 8 | p[1]);
	if (n)
		sum += (uint32_t)(p[0] << 8);
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return (uint16_t)~sum;
}

static uint32_t pseudo(uint32_t s, uint32_t d, int proto, size_t len)
{
	return (s >> 16) + (s & 0xffff) + (d >> 16) + (d & 0xffff) + (uint32_t)proto + (uint32_t)len;
}

/* ---- frames over the socket ---- */

static void put_frame(const uint8_t *f, size_t n)
{
	uint8_t h[4];
	ssize_t r;

	wr32(h, (uint32_t)n);
	if (write(fd, h, 4) != 4 || (r = write(fd, f, n)) != (ssize_t)n) {
		fprintf(stderr, "nethard: the guest's card went away\n");
		exit(2);
	}
}

static int get_exact(uint8_t *b, size_t n)
{
	size_t got = 0;

	while (got < n) {
		ssize_t r = read(fd, b + got, n - got);

		if (r <= 0)
			return -1;
		got += (size_t)r;
	}
	return 0;
}

static void arp_reply(uint32_t spa, const uint8_t *tha, uint32_t tpa)
{
	uint8_t f[60];

	memset(f, 0, sizeof f);
	memcpy(f, tha, 6);
	memcpy(f + 6, HMAC, 6);
	wr16(f + 12, 0x0806);
	wr16(f + 14, 1), wr16(f + 16, 0x0800), f[18] = 6, f[19] = 4, wr16(f + 20, 2);
	memcpy(f + 22, HMAC, 6);
	wr32(f + 28, spa);
	memcpy(f + 32, tha, 6);
	wr32(f + 38, tpa);
	put_frame(f, 60);
}

/* the next frame within ms (0: none).  ARP requests for the addresses of
   10.9.0.0/24 are answered here; everything else is returned. */
static size_t next_frame(uint8_t *buf, size_t max, int ms)
{
	uint64_t end = now_ms() + (uint64_t)ms;

	for (;;) {
		struct pollfd p = {fd, POLLIN, 0};
		int left = (int)(end - now_ms());
		uint8_t h[4];
		uint32_t n;

		if (now_ms() >= end)
			return 0;
		if (poll(&p, 1, left) <= 0)
			return 0;
		if (get_exact(h, 4) || (n = rd32(h)) > max || get_exact(buf, n)) {
			fprintf(stderr, "nethard: the guest's card went away\n");
			exit(2);
		}
		if (n >= 42 && rd16(buf + 12) == 0x0806 && rd16(buf + 20) == 1) {
			uint32_t tpa = rd32(buf + 38), spa = rd32(buf + 28);

			if ((tpa >> 8) == (HIP >> 8) && tpa != GIP) {
				arp_reply(tpa, buf + 22, spa);
				continue;
			}
		}
		return n;
	}
}

static void drain(int ms)
{
	uint8_t b[2048];

	while (next_frame(b, sizeof b, ms))
		;
}

/* ---- IP ---- */

static uint8_t *gmac_for(uint32_t dst) { (void)dst; return (uint8_t *)GMAC; }

static void send_ip_to(const uint8_t *dmac, uint32_t src, uint32_t dst, int proto, unsigned id, unsigned fo, const uint8_t *opt,
    size_t optlen, const void *pl, size_t n)
{
	uint8_t f[14 + 60 + 1600];
	size_t hl = 20 + optlen;

	memset(f, 0, 14 + hl);
	memcpy(f, dmac, 6);
	memcpy(f + 6, HMAC, 6);
	wr16(f + 12, 0x0800);
	f[14] = (uint8_t)(0x40 | hl / 4);
	wr16(f + 16, (unsigned)(hl + n));
	wr16(f + 18, id);
	wr16(f + 20, fo);
	f[22] = 64;
	f[23] = (uint8_t)proto;
	wr32(f + 26, src);
	wr32(f + 30, dst);
	if (optlen)
		memcpy(f + 34, opt, optlen);
	wr16(f + 24, csum(f + 14, hl, 0));
	memcpy(f + 14 + hl, pl, n);
	put_frame(f, 14 + hl + n);
}

static void send_ip(uint32_t src, uint32_t dst, int proto, unsigned id, unsigned fo, const uint8_t *opt, size_t optlen,
    const void *pl, size_t n)
{
	send_ip_to(gmac_for(dst), src, dst, proto, id, fo, opt, optlen, pl, n);
}

static size_t icmp_echo(uint8_t *m, unsigned id, unsigned seq, size_t n)
{
	memset(m, 0, 8 + n);
	m[0] = 8;
	wr16(m + 4, id);
	wr16(m + 6, seq);
	for (size_t i = 0; i < n; i++)
		m[8 + i] = (uint8_t)(i * 7 + 1);
	wr16(m + 2, csum(m, 8 + n, 0));
	return 8 + n;
}

/* an IP frame for us from the guest: its protocol and the payload */
static int ip_from_guest(const uint8_t *f, size_t n, int proto, const uint8_t **pl, size_t *pn, uint32_t *src)
{
	if (n < 34 || rd16(f + 12) != 0x0800 || f[23] != proto || rd32(f + 30) == 0)
		return 0;
	size_t hl = (size_t)(f[14] & 15) * 4, tl = rd16(f + 16);

	if (hl < 20 || tl < hl || 14 + tl > n) /* (a frame may have padding after the datagram) */
		return 0;
	if (pl)
		*pl = f + 14 + hl;
	if (pn)
		*pn = tl - hl;
	if (src)
		*src = rd32(f + 26);
	return 1;
}

/* a reply to an echo request that carries id: seen within ms? */
static int echo_reply(unsigned id, int ms)
{
	uint8_t f[2048];
	size_t n;
	uint64_t end = now_ms() + (uint64_t)ms;

	while ((int)(end - now_ms()) > 0 && (n = next_frame(f, sizeof f, (int)(end - now_ms()))) != 0) {
		const uint8_t *p;
		size_t pn;

		if (ip_from_guest(f, n, 1, &p, &pn, NULL) && pn >= 8 && p[0] == 0 && rd16(p + 4) == id)
			return 1;
	}
	return 0;
}

static int ping(uint32_t src, uint32_t dst, unsigned id, const uint8_t *opt, size_t optlen, const uint8_t *dmac)
{
	uint8_t m[64];
	size_t n = icmp_echo(m, id, 1, 16);

	drain(20);
	send_ip_to(dmac, src, dst, 1, id, 0, opt, optlen, m, n);
	return echo_reply(id, 400);
}

/* ---- TCP ---- */

struct conn {
	uint32_t lip;
	uint16_t lport, rport;
	uint32_t snd_nxt, rcv_nxt;
	unsigned gmss;
};

struct seg {
	int flags;
	uint32_t seq, ack;
	unsigned win;
	size_t dlen;
	uint8_t data[2048];
	uint16_t sport, dport;
	uint32_t src, dst;
	unsigned mss;
};

#define FIN 1
#define SYN 2
#define RST 4
#define PSH 8
#define ACK 16

static void tcp_raw(uint32_t src, uint16_t sport, uint16_t dport, uint32_t seq, uint32_t ack, int flags, unsigned win, unsigned mss,
    const void *data, size_t n)
{
	uint8_t s[24 + 1500];
	size_t hl = mss ? 24 : 20;

	memset(s, 0, hl);
	wr16(s, sport), wr16(s + 2, dport), wr32(s + 4, seq), wr32(s + 8, ack);
	s[12] = (uint8_t)(hl / 4 << 4);
	s[13] = (uint8_t)flags;
	wr16(s + 14, win);
	if (mss)
		s[20] = 2, s[21] = 4, wr16(s + 22, mss);
	if (n)
		memcpy(s + hl, data, n);
	wr16(s + 16, csum(s, hl + n, pseudo(src, GIP, 6, hl + n)));
	send_ip(src, GIP, 6, (unsigned)rand() & 0xffff, 0x4000, NULL, 0, s, hl + n);
}

/* the next TCP segment from the guest (to c's ports, or any) within ms */
static int tcp_next(struct seg *s, const struct conn *c, int ms)
{
	uint8_t f[2048];
	size_t n;
	uint64_t end = now_ms() + (uint64_t)ms;

	while ((int)(end - now_ms()) > 0 && (n = next_frame(f, sizeof f, (int)(end - now_ms()))) != 0) {
		const uint8_t *p;
		size_t pn;
		uint32_t src;

		if (!ip_from_guest(f, n, 6, &p, &pn, &src) || pn < 20)
			continue;
		size_t hl = (size_t)(p[12] >> 4) * 4;

		if (hl < 20 || hl > pn)
			continue;
		if (c && (rd16(p + 2) != c->lport || rd16(p) != c->rport))
			continue;
		s->flags = p[13], s->seq = rd32(p + 4), s->ack = rd32(p + 8), s->win = rd16(p + 14);
		s->sport = rd16(p), s->dport = rd16(p + 2), s->src = src, s->dst = rd32(f + 30);
		s->dlen = pn - hl > sizeof s->data ? sizeof s->data : pn - hl;
		memcpy(s->data, p + hl, s->dlen);
		s->mss = 0;
		if (getenv("NH_DEBUG"))
			fprintf(stderr, "# tcp from guest %u>%u flags %02x seq %u ack %u win %u dlen %zu\n", s->sport, s->dport, s->flags,
			    s->seq, s->ack, s->win, s->dlen);
		for (size_t i = 20; i + 4 <= hl && p[i];) {
			if (p[i] == 1) {
				i++;
				continue;
			}
			if (p[i] == 2 && p[i + 1] == 4)
				s->mss = rd16(p + i + 2);
			if (p[i + 1] < 2)
				break;
			i += p[i + 1];
		}
		return 1;
	}
	return 0;
}

static void tcp_out(struct conn *c, int flags, const void *data, size_t n)
{
	tcp_raw(c->lip, c->lport, c->rport, c->snd_nxt, c->rcv_nxt, flags, 8192, 0, data, n);
	c->snd_nxt += (uint32_t)n + ((flags & (SYN | FIN)) ? 1 : 0);
}

/* connect from lip:lport to the guest's port, with the MSS option mss (0: none sent) */
static int tcp_open(struct conn *c, uint32_t lip, uint16_t lport, uint16_t rport, unsigned mss)
{
	struct seg s;
	uint32_t iss = (uint32_t)rand() * 2654435761U;

	memset(c, 0, sizeof *c);
	c->lip = lip, c->lport = lport, c->rport = rport;
	for (int tries = 0; tries < 3; tries++) {
		drain(10);
		tcp_raw(lip, lport, rport, iss, 0, SYN, 8192, mss, NULL, 0);
		if (tcp_next(&s, c, 700) && (s.flags & (SYN | ACK)) == (SYN | ACK) && s.ack == iss + 1) {
			c->snd_nxt = iss + 1;
			c->rcv_nxt = s.seq + 1;
			c->gmss = s.mss;
			tcp_out(c, ACK, NULL, 0);
			return 0;
		}
		if (s.flags & RST)
			usleep(300000);
	}
	return -1;
}

/* send data and read it back (the echo), acknowledging; returns the bytes */
static size_t tcp_echo(struct conn *c, const void *data, size_t n, unsigned *maxseg)
{
	static uint8_t back[1 << 17];
	size_t got = 0;
	struct seg s;
	const uint8_t *d = data;
	size_t sent = 0;

	if (maxseg)
		*maxseg = 0;
	while (sent < n) { /* (not more than a window at a time) */
		size_t k = n - sent > 1400 ? 1400 : n - sent;

		tcp_out(c, ACK | PSH, d + sent, k);
		sent += k;
		while (tcp_next(&s, c, sent == n ? 1500 : 5)) {
			if (s.dlen && s.seq == c->rcv_nxt) {
				c->rcv_nxt += (uint32_t)s.dlen;
				if (got + s.dlen <= sizeof back)
					memcpy(back + got, s.data, s.dlen);
				got += s.dlen;
				if (maxseg && s.dlen > *maxseg)
					*maxseg = (unsigned)s.dlen;
				tcp_out(c, ACK, NULL, 0);
			}
			if (got >= n)
				break;
		}
	}
	while (got < n && tcp_next(&s, c, 1500)) {
		if (s.dlen && s.seq == c->rcv_nxt) {
			c->rcv_nxt += (uint32_t)s.dlen;
			if (got + s.dlen <= sizeof back)
				memcpy(back + got, s.data, s.dlen);
			got += s.dlen;
			if (maxseg && s.dlen > *maxseg)
				*maxseg = (unsigned)s.dlen;
			tcp_out(c, ACK, NULL, 0);
		}
	}
	if (got == n && n <= sizeof back && memcmp(back, data, n))
		return 0;
	return got;
}

static int alive(struct conn *c)
{
	return tcp_echo(c, "alive?", 6, NULL) == 6;
}

/* ---- the tests ---- */

static void t_arp(void)
{
	uint8_t f[60], b[2048];
	size_t n;

	/* a request for the guest from a neighbour: answered */
	memset(f, 0, sizeof f);
	memcpy(f, BMAC, 6), memcpy(f + 6, HMAC, 6), wr16(f + 12, 0x0806);
	wr16(f + 14, 1), wr16(f + 16, 0x0800), f[18] = 6, f[19] = 4, wr16(f + 20, 1);
	memcpy(f + 22, HMAC, 6), wr32(f + 28, HIP), wr32(f + 38, GIP);
	drain(20);
	put_frame(f, 60);
	int got = 0;

	while ((n = next_frame(b, sizeof b, 500)) != 0)
		if (n >= 42 && rd16(b + 12) == 0x0806 && rd16(b + 20) == 2 && rd32(b + 28) == GIP)
			got = 1;
	check(got, "arp: a request for the guest is answered");

	/* the same, claiming to be the guest's own address: not answered (and logged by the guest) */
	wr32(f + 28, GIP);
	drain(20);
	put_frame(f, 60);
	got = 0;
	while ((n = next_frame(b, sizeof b, 400)) != 0)
		if (n >= 42 && rd16(b + 12) == 0x0806 && rd16(b + 20) == 2)
			got = 1;
	check(!got, "arp: a request from our own address is not answered");

	/* sender hardware address is a group address: not answered */
	wr32(f + 28, HIP);
	f[22] = 0x01;
	drain(20);
	put_frame(f, 60);
	got = 0;
	while ((n = next_frame(b, sizeof b, 400)) != 0)
		if (n >= 42 && rd16(b + 12) == 0x0806 && rd16(b + 20) == 2)
			got = 1;
	check(!got, "arp: a request from a group hardware address is not answered");

	/* a frame shorter than an Ethernet header, and one that is just a header: no harm */
	put_frame(f, 5);
	put_frame(f, 14);
	check(ping(HIP, GIP, 0x1101, NULL, 0, GMAC), "arp: the guest lives after tiny frames");
}

static void t_ip(void)
{
	check(ping(HIP, GIP, 0x2001, NULL, 0, GMAC), "ip: an echo request is answered");
	check(!ping(HIP, BCAST, 0x2002, NULL, 0, BMAC), "ip: an echo request to a broadcast address is not");
	check(!ping(HIP, 0xffffffffU, 0x2003, NULL, 0, BMAC), "ip: nor one to 255.255.255.255");
	check(!ping(0, GIP, 0x2004, NULL, 0, GMAC), "ip: a source of 0.0.0.0 gets no answer");
	check(!ping(GIP, GIP, 0x2005, NULL, 0, GMAC), "ip: a source that is the guest's own address gets none");
	check(!ping(0xe0000001U, GIP, 0x2006, NULL, 0, GMAC), "ip: a multicast source gets none");
	check(!ping(0xffffffffU, GIP, 0x2007, NULL, 0, GMAC), "ip: source 255.255.255.255 gets none");
	check(!ping(0x7f000001U, GIP, 0x2008, NULL, 0, GMAC), "ip: a loopback source gets none");
	check(!ping(BCAST, GIP, 0x2009, NULL, 0, GMAC), "ip: a source that is the network's broadcast address gets none");
	check(!ping(0xf0000001U, GIP, 0x200a, NULL, 0, GMAC), "ip: a class E source gets none");
	{
		static const uint8_t lsrr[8] = {131, 7, 4, 10, 9, 0, 1, 0}, ssrr[8] = {137, 7, 4, 10, 9, 0, 1, 0};
		static const uint8_t rr[8] = {7, 7, 4, 0, 0, 0, 0, 1}, nop[4] = {1, 1, 1, 0}, bad[4] = {68, 1, 0, 0}, bad2[4] = {68, 9, 0, 0};

		check(!ping(HIP, GIP, 0x2010, lsrr, 8, GMAC), "ip: a loose source route is dropped");
		check(!ping(HIP, GIP, 0x2011, ssrr, 8, GMAC), "ip: a strict source route is dropped");
		check(ping(HIP, GIP, 0x2012, rr, 8, GMAC), "ip: record route is harmless and answered");
		check(ping(HIP, GIP, 0x2013, nop, 4, GMAC), "ip: NOPs and the end of options are fine");
		check(!ping(HIP, GIP, 0x2014, bad, 4, GMAC), "ip: an option of length 1 is dropped");
		check(!ping(HIP, GIP, 0x2015, bad2, 4, GMAC), "ip: an option longer than the header is dropped");
	}
	check(ping(HIP, GIP, 0x2020, NULL, 0, GMAC), "ip: still alive");
}

/* an echo request of icmp bytes in fragments of at most per bytes of payload; the
   fragments' order is given (0: forward, 1: backward); returns the reply's bytes (0: none) */
static size_t frag_echo(unsigned id, size_t icmp, size_t per, int backward, size_t *nfrag)
{
	static uint8_t m[70000];
	uint8_t f[2048];
	size_t n = icmp_echo(m, id, 1, icmp - 8), total = 0, got, k = 0;
	size_t nf = (n + per - 1) / per;

	drain(20);
	for (size_t i = 0; i < nf; i++) {
		size_t j = backward ? nf - 1 - i : i, off = j * per, len = n - off > per ? per : n - off;

		send_ip(HIP, GIP, 1, id, (unsigned)((j + 1 < nf ? 0x2000 : 0) | (off >> 3)), NULL, 0, m + off, len);
	}
	/* the reply comes in fragments too: count what comes with the id of its first */
	uint64_t end = now_ms() + 3000;
	uint16_t rid = 0;
	int have_first = 0;

	got = 0;
	while (now_ms() < end) {
		size_t r = next_frame(f, sizeof f, 300);

		if (r < 34 || rd16(f + 12) != 0x0800 || f[23] != 1 || rd32(f + 30) != HIP || rd32(f + 26) != GIP)
			continue;
		size_t hl = (size_t)(f[14] & 15) * 4, pl = rd16(f + 16) - hl;
		unsigned fo = rd16(f + 20);

		if (!(fo & 0x1fff)) {
			if (r < 14 + hl + 8 || f[14 + hl] != 0 || rd16(f + 14 + hl + 4) != id)
				continue;
			rid = rd16(f + 18), have_first = 1;
		} else if (!have_first || rd16(f + 18) != rid)
			continue;
		k++;
		got += pl;
		if (!(fo & 0x2000)) {
			total = ((fo & 0x1fff) * 8) + pl;
			break;
		}
	}
	if (nfrag)
		*nfrag = k;
	(void)got;
	return total;
}

static void t_frag(void)
{
	size_t nf, r;

	r = frag_echo(0x3001, 3000, 1480, 0, &nf);
	check(r == 3000 && nf == 3, "frag: a datagram in 3 fragments is reassembled and answered");
	r = frag_echo(0x3002, 3000, 1480, 1, &nf);
	check(r == 3000, "frag: the same with the fragments in reverse order");
	r = frag_echo(0x3003, 65515, 1480, 0, &nf);
	check(r == 65515, "frag: the largest datagram (65515 of ICMP) is reassembled");
	{ /* the last fragment alone, at the very end of what fits (it used to be written past the buffer) */
		uint8_t tail[40];

		memset(tail, 0x5a, sizeof tail);
		send_ip(HIP, GIP, 1, 0x3004, 65472 >> 3, NULL, 0, tail, 40);
		usleep(100000);
	}
	check(ping(HIP, GIP, 0x3005, NULL, 0, GMAC), "frag: still alive");

	/* overlap: the second fragment repeats part of the first with other bytes */
	{
		static uint8_t m[4000];
		size_t n = icmp_echo(m, 0x3010, 1, 1992);
		uint8_t over[400];

		drain(20);
		send_ip(HIP, GIP, 1, 0x3010, 0x2000, NULL, 0, m, 1480);
		memset(over, 0xaa, sizeof over);
		send_ip(HIP, GIP, 1, 0x3010, 0x2000 | (1400 >> 3), NULL, 0, over, 400); /* 1400..1800 overlaps 1400..1480 */
		send_ip(HIP, GIP, 1, 0x3010, 1800 >> 3, NULL, 0, m + 1800, n - 1800);
		check(!echo_reply(0x3010, 800), "frag: overlapping fragments with different bytes drop the datagram");
	}
	/* a duplicate fragment (the same bytes) is harmless */
	{
		static uint8_t m[4000];
		size_t n = icmp_echo(m, 0x3011, 1, 1992);

		drain(20);
		send_ip(HIP, GIP, 1, 0x3011, 0x2000, NULL, 0, m, 1480);
		send_ip(HIP, GIP, 1, 0x3011, 0x2000, NULL, 0, m, 1480);
		send_ip(HIP, GIP, 1, 0x3011, 1480 >> 3, NULL, 0, m + 1480, n - 1480);
		check(echo_reply(0x3011, 1500), "frag: a repeated fragment is harmless");
	}
	/* two different ends: the tails of two echo requests, the second shorter; then the head of the second: without the check
	   the second would be complete and answered */
	{
		static uint8_t m1[2000], m2[2000];

		icmp_echo(m1, 0x3012, 1, 992);	/* 1000 bytes */
		icmp_echo(m2, 0x3012, 1, 588);	/* 596 bytes */
		drain(20);
		send_ip(HIP, GIP, 1, 0x3012, 496 >> 3, NULL, 0, m1 + 496, 504);
		send_ip(HIP, GIP, 1, 0x3012, 496 >> 3, NULL, 0, m2 + 496, 100);
		send_ip(HIP, GIP, 1, 0x3012, 0x2000, NULL, 0, m2, 496);
		check(!echo_reply(0x3012, 700), "frag: two different ends drop the datagram");
		send_ip(HIP, GIP, 1, 0x3013, 496 >> 3, NULL, 0, m2 + 496, 100);
		send_ip(HIP, GIP, 1, 0x3013, 0x2000, NULL, 0, m2, 496);
		check(echo_reply(0x3012, 700), "frag: (control: the shorter one alone is answered)");
	}
	/* a flood of first fragments of other datagrams does not keep the table full */
	{
		static uint8_t m[4000];
		size_t n = icmp_echo(m, 0x3020, 1, 1992);

		drain(20);
		for (unsigned i = 0; i < 40; i++)
			send_ip(0x0a090010U + i, GIP, 1, 0x4000 + i, 0x2000, NULL, 0, m, 64);
		usleep(100000);
		send_ip(HIP, GIP, 1, 0x3020, 0x2000, NULL, 0, m, 1480);
		send_ip(HIP, GIP, 1, 0x3020, 1480 >> 3, NULL, 0, m + 1480, n - 1480);
		check(echo_reply(0x3020, 1500), "frag: a flood of first fragments does not stop a real datagram");
	}
	check(ping(HIP, GIP, 0x3030, NULL, 0, GMAC), "frag: still alive");
}

static void t_ratelimit(void)
{
	uint8_t u[16], f[2048];
	int seen = 0;
	size_t n;

	memset(u, 0, sizeof u);
	wr16(u, 5000), wr16(u + 2, 9), wr16(u + 4, 8);
	drain(50);
	for (int i = 0; i < 400; i++) {
		wr16(u + 6, 0);
		send_ip(HIP, GIP, 17, (unsigned)i + 1, 0, NULL, 0, u, 8);
	}
	while ((n = next_frame(f, sizeof f, 700)) != 0) {
		const uint8_t *p;
		size_t pn;

		if (ip_from_guest(f, n, 1, &p, &pn, NULL) && pn >= 8 && p[0] == 3 && p[1] == 3)
			seen++;
	}
	printf("# %d port unreachables for 400 datagrams to a closed port\n", seen);
	check(seen >= 1 && seen <= 200, "icmp: errors for datagrams to a closed port are limited (about 100 a second)");
}

static void t_tcp(void)
{
	struct conn c, c2;
	struct seg s;
	unsigned maxseg;
	char big[3000];

	check(!tcp_open(&c, HIP, 30001, 7000, 1460) && alive(&c), "tcp: a handshake and an echo");
	tcp_out(&c, FIN | ACK, NULL, 0);
	drain(300);

	/* an MSS of 0 must not hang or crash the guest: segments are at least 216 bytes */
	memset(big, 'm', sizeof big);
	{
		/* (tcp_raw sends mss only when nonzero: build the 0 option by hand) */
		uint8_t sp[24];
		uint32_t iss = 0x1000;

		memset(sp, 0, sizeof sp);
		wr16(sp, 30003), wr16(sp + 2, 7000), wr32(sp + 4, iss);
		sp[12] = 6 << 4, sp[13] = SYN;
		wr16(sp + 14, 8192);
		sp[20] = 2, sp[21] = 4, wr16(sp + 22, 0);
		wr16(sp + 16, csum(sp, 24, pseudo(HIP, GIP, 6, 24)));
		drain(20);
		send_ip(HIP, GIP, 6, 77, 0x4000, NULL, 0, sp, 24);
		memset(&c, 0, sizeof c);
		c.lip = HIP, c.lport = 30003, c.rport = 7000;
		if (tcp_next(&s, &c, 800) && (s.flags & (SYN | ACK)) == (SYN | ACK)) {
			c.snd_nxt = iss + 1, c.rcv_nxt = s.seq + 1;
			tcp_out(&c, ACK, NULL, 0);
			size_t got = tcp_echo(&c, big, 1000, &maxseg);

			check(got == 1000, "tcp: a peer with MSS 0: the guest still sends its bytes");
			check(maxseg >= 216 && maxseg <= 536, "tcp: ...in segments of the least size (216), not of 0");
		} else
			check(0, "tcp: a peer with MSS 0: no handshake");
		tcp_out(&c, FIN | ACK, NULL, 0);
		drain(300);
	}

	/* blind resets and SYNs (RFC 5961) */
	check(!tcp_open(&c, HIP, 30010, 7000, 1460) && alive(&c), "tcp: a connection to attack");
	drain(50);
	tcp_raw(HIP, 30010, 7000, c.snd_nxt + 1000, 0, RST | ACK, 0, 0, NULL, 0);
	{
		int challenged = 0;

		while (tcp_next(&s, &c, 400))
			if ((s.flags & (RST | SYN | FIN)) == 0 && s.ack == c.snd_nxt && !s.dlen)
				challenged = 1;
		check(challenged, "tcp: a reset inside the window but not at the next number gets a challenge ACK");
	}
	check(alive(&c), "tcp: ...and the connection lives");
	tcp_raw(HIP, 30010, 7000, c.snd_nxt + 5, 0, RST, 0, 0, NULL, 0);
	check(alive(&c), "tcp: a reset with no ACK flag, off by 5, changes nothing either");
	tcp_raw(HIP, 30010, 7000, c.snd_nxt + 20, c.rcv_nxt, SYN | ACK, 8192, 0, NULL, 0);
	drain(300);
	check(alive(&c), "tcp: a SYN inside the window does not reset the connection");
	tcp_raw(HIP, 30010, 7000, c.snd_nxt, c.rcv_nxt + 100000, ACK, 8192, 0, "x", 1);
	{
		int challenged = 0;

		while (tcp_next(&s, &c, 400))
			if (!s.dlen && (s.flags & RST) == 0 && s.ack == c.snd_nxt)
				challenged = 1;
		check(challenged, "tcp: an ACK for what was never sent gets a challenge ACK");
	}
	check(alive(&c), "tcp: ...and the connection lives");
	tcp_raw(HIP, 30010, 7000, c.snd_nxt, c.rcv_nxt - 200000, ACK, 8192, 0, "x", 1);
	drain(300);
	check(alive(&c), "tcp: an ACK that is far too old is not taken for data");
	/* the right reset ends it */
	tcp_raw(HIP, 30010, 7000, c.snd_nxt, c.rcv_nxt, RST | ACK, 0, 0, NULL, 0);
	drain(200);
	tcp_raw(HIP, 30010, 7000, c.snd_nxt, c.rcv_nxt, ACK | PSH, 8192, 0, "after", 5);
	{
		int rst = 0;

		while (tcp_next(&s, NULL, 400))
			if (s.flags & RST)
				rst = 1;
		check(rst, "tcp: a reset at exactly the next number ends the connection");
	}

	/* bad flag combinations to a listening port get nothing, a SYN with port 0 too */
	drain(400);
	tcp_raw(HIP, 30020, 7000, 5000, 0, SYN | FIN, 8192, 0, NULL, 0);
	tcp_raw(HIP, 0, 7000, 5000, 0, SYN, 8192, 0, NULL, 0);
	tcp_raw(HIP, 30021, 7000, 5000, 0, SYN | RST, 8192, 0, NULL, 0);
	{
		int answered = 0;

		while (tcp_next(&s, NULL, 500))
			if (s.dport == 30020 || s.dport == 30021 || s.dport == 0 || s.sport == 0)
				answered = 1;
		check(!answered, "tcp: SYN+FIN, SYN+RST and port 0 get no answer");
	}

	/* the initial sequence numbers: new connections differ, and the same four go forward with the clock */
	{
		uint32_t a[8];
		int n = 0, bad = 0;

		for (int i = 0; i < 8; i++) {
			memset(&c2, 0, sizeof c2);
			c2.lip = HIP, c2.lport = (uint16_t)(31000 + i), c2.rport = 7000;
			drain(10);
			tcp_raw(HIP, c2.lport, 7000, 100, 0, SYN, 8192, 0, NULL, 0);
			if (tcp_next(&s, &c2, 600) && (s.flags & (SYN | ACK)) == (SYN | ACK)) {
				a[n++] = s.seq;
				tcp_raw(HIP, c2.lport, 7000, 101, s.seq + 1, RST | ACK, 0, 0, NULL, 0);
			}
		}
		for (int i = 1; i < n; i++) {
			int32_t d = (int32_t)(a[i] - a[i - 1]);

			if (d > -100000 && d < 100000)
				bad++;
		}
		check(n == 8 && bad == 0, "tcp: the sequence numbers of different connections are far apart");
		uint32_t x1 = 0, x2 = 0;

		for (int i = 0; i < 2; i++) {
			drain(10);
			tcp_raw(HIP, 31100, 7000, 100, 0, SYN, 8192, 0, NULL, 0);
			if (tcp_next(&s, NULL, 600) && (s.flags & (SYN | ACK)) == (SYN | ACK)) {
				{ if (i) x2 = s.seq; else x1 = s.seq; }
				tcp_raw(HIP, 31100, 7000, 101, s.seq + 1, RST | ACK, 0, 0, NULL, 0);
			}
			usleep(1000000);
		}
		uint32_t d = x2 - x1;

		printf("# the same four a second apart: %u ticks\n", d);
		check(x1 && x2 && d > 150000 && d < 400000, "tcp: the same four a second later: the number is 250000 ticks on (RFC 6528)");
	}

	/* closed port: resets, limited */
	{
		int rsts = 0;

		drain(50);
		for (int i = 0; i < 400; i++)
			tcp_raw(HIP, (uint16_t)(33000 + i), 7999, 1, 0, SYN, 8192, 0, NULL, 0);
		while (tcp_next(&s, NULL, 700))
			if (s.flags & RST)
				rsts++;
		printf("# %d resets for 400 SYNs to a closed port\n", rsts);
		check(rsts >= 1 && rsts <= 200, "tcp: resets for segments to a closed port are limited (about 100 a second)");
	}
	drain(1100);

	/* segments that wait for a gap: a lot of tiny ones do not hurt */
	check(!tcp_open(&c, HIP, 30030, 7000, 1460) && alive(&c), "tcp: a connection for the gap test");
	for (int i = 0; i < 200; i++)
		tcp_raw(HIP, 30030, 7000, c.snd_nxt + 10 + (uint32_t)i * 3, c.rcv_nxt, ACK | PSH, 8192, 0, "z", 1);
	drain(500);
	check(alive(&c), "tcp: 200 one byte segments ahead of a gap do not hurt the connection");
	check(ping(HIP, GIP, 0x5001, NULL, 0, GMAC), "tcp: the guest is alive");
	tcp_out(&c, FIN | ACK, NULL, 0);
	drain(300);
}

struct flood {
	uint32_t src[200];
	uint32_t seq[200];
	uint32_t isn[200];
	int n;
};

static void t_syn(void)
{
	struct flood fl;
	struct seg s;
	struct conn c;
	int answered = 0;

	memset(&fl, 0, sizeof fl);
	drain(50);
	/* 150 SYNs from 150 addresses that are nobody's: the guest has to answer them all, with cookies for most */
	{
		static uint8_t seen[256];

		for (int i = 0; i < 150; i++) {
			fl.src[i] = 0x0a09000aU + (uint32_t)i;
			fl.seq[i] = 0x20000000U + (uint32_t)i * 1000;
			tcp_raw(fl.src[i], 40000, 7000, fl.seq[i], 0, SYN, 8192, 1460, NULL, 0);
			if (i % 8 == 7) {
				uint64_t end = now_ms() + 150;

				while ((int)(end - now_ms()) > 0 && tcp_next(&s, NULL, (int)(end - now_ms())))
					if ((s.flags & (SYN | ACK)) == (SYN | ACK) && s.dport == 40000 && !seen[s.dst & 255])
						seen[s.dst & 255] = 1, answered++;
			}
		}
		uint64_t end = now_ms() + 2000;

		while (answered < 150 && now_ms() < end && tcp_next(&s, NULL, 500))
			if ((s.flags & (SYN | ACK)) == (SYN | ACK) && s.dport == 40000 && !seen[s.dst & 255])
				seen[s.dst & 255] = 1, answered++;
	}
	printf("# %d SYN-ACKs for 150 SYNs\n", answered);
	check(answered >= 140, "syn: 150 SYNs from 150 addresses are all answered (cookies once 16 are going)");

	/* the ACK of a cookie makes the connection: take the last SYN's, which was surely a cookie */
	{
		int ok = 0;

		/* ask once more from a new address and complete its handshake */
		uint32_t src = 0x0a0900f0U, iss = 0x30000000U;
		struct conn k;

		memset(&k, 0, sizeof k);
		k.lip = src, k.lport = 41000, k.rport = 7000;
		drain(20);
		tcp_raw(src, 41000, 7000, iss, 0, SYN, 8192, 1460, NULL, 0);
		if (tcp_next(&s, &k, 800) && (s.flags & (SYN | ACK)) == (SYN | ACK) && s.ack == iss + 1) {
			k.snd_nxt = iss + 1, k.rcv_nxt = s.seq + 1;
			printf("# the SYN-ACK says MSS %u\n", s.mss);
			tcp_out(&k, ACK, NULL, 0);
			ok = alive(&k);
			if (ok) {
				unsigned maxseg;
				char big[4000];

				memset(big, 'c', sizeof big);
				ok = tcp_echo(&k, big, 3000, &maxseg) == 3000;
				printf("# the largest segment of 3000 bytes: %u\n", maxseg);
				ok = ok && maxseg <= 1460 && maxseg > 216;
			}
		}
		check(ok, "syn: the ACK of a SYN cookie makes a working connection");

		/* a forged ACK (a cookie that was never given) gets a reset, not a connection */
		memset(&k, 0, sizeof k);
		drain(20);
		tcp_raw(0x0a0900f1U, 41001, 7000, 777, 0x12345678, ACK, 8192, 0, NULL, 0);
		int rst = 0;

		while (tcp_next(&s, NULL, 500))
			if (s.flags & RST)
				rst = 1;
		check(rst, "syn: an ACK with a cookie that was never given is reset");
	}
	/* the listener is still open to a real peer after the flood */
	check(!tcp_open(&c, HIP, 30050, 7000, 1460) && alive(&c), "syn: a real peer connects while the flood's SYN_RECEIVED entries wait");
	tcp_out(&c, FIN | ACK, NULL, 0);
	drain(300);
	check(ping(HIP, GIP, 0x6001, NULL, 0, GMAC), "syn: the guest is alive");
}

/* the guest connects to us: SYNs arrive at port rport; give back what the round wants */
static void t_connect(void)
{
	struct seg s;
	uint16_t ports[8];
	int n = 0;

	/* ports of the guest's connections: not in turn (RFC 6056) */
	for (int i = 0; i < 8; i++) {
		struct conn c;
		uint64_t end;

		memset(&c, 0, sizeof c);
		if (tcp_open(&c, HIP, (uint16_t)(20000 + rand() % 20000), 7002, 0))
			break;
		tcp_out(&c, ACK | PSH, "CONNECT 9001\n", 13);
		end = now_ms() + 5000;
		while (now_ms() < end && tcp_next(&s, NULL, 500)) {
			if ((s.flags & (SYN | ACK)) == SYN && s.dport == 9001) {
				ports[n++] = s.sport;
				tcp_raw(HIP, 9001, s.sport, 0, s.seq + 1, RST | ACK, 0, 0, NULL, 0); /* refused */
				break;
			}
		}
		drain(300);
	}
	{
		int turn = 0;

		for (int i = 1; i < n; i++)
			if ((uint16_t)(ports[i] - ports[i - 1]) == 1)
				turn++;
		printf("# the guest's ports:");
		for (int i = 0; i < n; i++)
			printf(" %u", ports[i]);
		printf("\n");
		check(n == 8 && turn < 3 && ports[0] >= 32768, "port: the guest's ports are not in turn");
	}

	/* an ICMP port unreachable with the wrong sequence number does not end a connect(); with the right one it does */
	{
		struct conn c;
		uint64_t end;
		uint32_t seq = 0;
		uint16_t sport = 0;

		memset(&c, 0, sizeof c);
		if (tcp_open(&c, HIP, 29999, 7002, 0)) {
			check(0, "icmp: no control connection");
			return;
		}
		tcp_out(&c, ACK | PSH, "CONNECT 9002\n", 13);
		end = now_ms() + 5000;
		while (now_ms() < end && tcp_next(&s, NULL, 500))
			if ((s.flags & (SYN | ACK)) == SYN && s.dport == 9002) {
				seq = s.seq, sport = s.sport;
				break;
			}
		if (!sport) {
			check(0, "icmp: the guest did not connect");
			return;
		}
		/* an ICMP unreachable about that SYN, with the sequence number moved */
		for (int round = 0; round < 2; round++) {
			uint8_t m[8 + 20 + 8], ipx[20];
			uint8_t tp[8];

			memset(m, 0, sizeof m);
			m[0] = 3, m[1] = 3;
			/* the packet that is quoted: from the guest to us */
			memset(ipx, 0, sizeof ipx);
			ipx[0] = 0x45, wr16(ipx + 2, 44), ipx[8] = 64, ipx[9] = 6;
			wr32(ipx + 12, GIP), wr32(ipx + 16, HIP);
			wr16(ipx + 10, csum(ipx, 20, 0));
			wr16(tp, sport), wr16(tp + 2, 9002), wr32(tp + 4, round == 0 ? seq + 5000 : seq);
			memcpy(m + 8, ipx, 20);
			memcpy(m + 28, tp, 8);
			wr16(m + 2, csum(m, sizeof m, 0));
			send_ip(HIP, GIP, 1, 900 + (unsigned)round, 0, NULL, 0, m, sizeof m);
			if (round == 0) { /* (still trying: the SYN comes again, in a second or so) */
				int again = 0;
				uint64_t e3 = now_ms() + 4000;

				while (!again && now_ms() < e3)
					if (tcp_next(&s, NULL, 500) && (s.flags & (SYN | ACK)) == SYN && s.dport == 9002 && s.sport == sport)
						again = 1;
				check(again, "icmp: an error with a sequence number that is not in flight does not end connect()");
			}
		}
		/* the answer comes over the control connection */
		{
			char res[64];
			size_t k = 0;
			uint64_t e2 = now_ms() + 8000;

			res[0] = 0;
			while (now_ms() < e2 && tcp_next(&s, &c, 1000))
				if (s.dlen && s.seq == c.rcv_nxt) {
					c.rcv_nxt += (uint32_t)s.dlen;
					if (k + s.dlen < sizeof res)
						memcpy(res + k, s.data, s.dlen), k += s.dlen;
					res[k] = 0;
					tcp_out(&c, ACK, NULL, 0);
					break;
				}
			printf("# connect() answered: %s", res);
			check(!strncmp(res, "ERR ECONNREFUSED", 16), "icmp: the guest's connect() ends with the right sequence number in the error");
		}
	}
}

static void quit_guest(void)
{
	struct conn c;
	struct seg s;

	if (!tcp_open(&c, HIP, 29000, 7002, 0)) {
		tcp_out(&c, ACK | PSH, "QUIT\n", 5);
		tcp_next(&s, &c, 1000);
	}
}

int main(int argc, char **argv)
{
	int ls, one = 1;
	struct sockaddr_in a;
	uint64_t t0;

	if (argc != 2) {
		fprintf(stderr, "usage: nethard port\n");
		return 2;
	}
	srand((unsigned)time(NULL));
	ls = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((unsigned short)atoi(argv[1]));
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(ls, (struct sockaddr *)&a, sizeof a) < 0 || listen(ls, 1) < 0) {
		perror("nethard: listen");
		return 2;
	}
	printf("nethard: waiting for the guest's card on 127.0.0.1:%s\n", argv[1]);
	fflush(stdout);
	{
		struct pollfd p = {ls, POLLIN, 0};

		if (poll(&p, 1, 120000) <= 0) {
			fprintf(stderr, "nethard: no guest\n");
			return 2;
		}
	}
	fd = accept(ls, NULL, NULL);
	if (fd < 0)
		return 2;
	/* wait for the guest to answer on its card and for hardsrv to listen */
	t0 = now_ms();
	for (;;) {
		struct conn c;

		if (now_ms() - t0 > 240000) {
			fprintf(stderr, "nethard: the guest did not come up\n");
			return 2;
		}
		if (!tcp_open(&c, HIP, 28000, 7000, 0)) {
			tcp_out(&c, FIN | ACK, NULL, 0);
			drain(300);
			break;
		}
		sleep(1);
	}
	printf("nethard: the guest is up\n");
	fflush(stdout);
	t_arp();
	t_ip();
	t_frag();
	t_ratelimit();
	t_tcp();
	t_syn();
	t_connect();
	check(ping(HIP, GIP, 0x7001, NULL, 0, GMAC), "the guest is alive at the end");
	quit_guest();
	printf("nethard: %d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
