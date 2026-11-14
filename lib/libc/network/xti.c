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

/* XTI (SUSv2) on sockets: /dev/tcp is a connection-mode endpoint with
   orderly release, /dev/udp a connectionless one.  A connection indication
   is a connection accepted by the kernel and held until t_accept or
   t_snddis; the endpoint of an ended connection is a new socket. */
#define _DEFAULT_SOURCE
#include <xti.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include "xti_impl.h"

struct pend {
	int sd, seq, dead, reason;
	struct sockaddr_in from;
};

struct ep {
	int fd, type, state;
	unsigned qlen;
	struct pend *pend;
	int npend, cap, nextseq;
	int flow, fin, disc, reason;
	struct sockaddr_in local, peer;
	unsigned char *rem;
	size_t remlen;
};

#define SIN sizeof(struct sockaddr_in)
#define MAXUDP 65507

static struct ep **tab;
static int ntab;
static pthread_mutex_t tablock = PTHREAD_MUTEX_INITIALIZER;

static int fail(int e)
{
	t_errno = e;
	return -1;
}

static int sysfail(void)
{
	int e = errno;
	t_errno = TSYSERR;
	errno = e;
	return -1;
}

static struct ep *get(int fd)
{
	struct ep *e = 0;
	pthread_mutex_lock(&tablock);
	if (fd >= 0 && fd < ntab) e = tab[fd];
	pthread_mutex_unlock(&tablock);
	return e;
}

static struct ep *reg(int fd, int type, int state)
{
	struct ep *e = calloc(1, sizeof *e);
	if (!e) return 0;
	e->fd = fd; e->type = type; e->state = state;
	pthread_mutex_lock(&tablock);
	if (fd >= ntab) {
		struct ep **n = realloc(tab, (fd + 16) * sizeof *tab);
		if (!n) {
			pthread_mutex_unlock(&tablock);
			free(e);
			return 0;
		}
		memset(n + ntab, 0, (fd + 16 - ntab) * sizeof *n);
		tab = n;
		ntab = fd + 16;
	}
	free(tab[fd]);
	tab[fd] = e;
	pthread_mutex_unlock(&tablock);
	return e;
}

static int nonblock(int fd)
{
	int f = fcntl(fd, F_GETFL);
	return f >= 0 && (f & O_NONBLOCK);
}

/* revents of fd after waiting TMO ms (-1: for ever); 0 on time-out, -1 on error */
static int waitfd(int fd, short events, int tmo)
{
	struct pollfd p;
	int r;
	p.fd = fd; p.events = events; p.revents = 0;
	r = poll(&p, 1, tmo);
	return r <= 0 ? r : p.revents;
}

/* 1 data, 0 end of file, -1 nothing yet, -2 an error (errno) */
static int peek(int fd)
{
	char c;
	ssize_t n = recv(fd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
	if (n > 0) return 1;
	if (n == 0) return 0;
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? -1 : -2;
}

static void fill_info(int type, struct t_info *i)
{
	i->addr = SIN;
	i->options = 1024;
	i->etsdu = T_INVALID;
	i->connect = T_INVALID;
	i->discon = T_INVALID;
	if (type == SOCK_STREAM) {
		i->tsdu = 0;
		i->servtype = T_COTS_ORD;
		i->flags = 0;
	} else {
		i->tsdu = MAXUDP;
		i->servtype = T_CLTS;
		i->flags = T_SENDZERO;
	}
}

/* the connection-ending errors: a disconnect indication, not a failure */
static int is_disc(int e)
{
	return e == ECONNRESET || e == ECONNABORTED || e == ECONNREFUSED || e == EPIPE ||
	    e == ETIMEDOUT || e == ENOTCONN || e == ENETUNREACH || e == EHOSTUNREACH ||
	    e == ENETDOWN || e == EHOSTDOWN || e == ENETRESET;
}

/* the event waiting at E, or 0 */
static int event(struct ep *e)
{
	int fd = e->fd, r, i, err;
	socklen_t l = sizeof err;

	switch (e->state) {
	case T_UNBND:
		return 0;
	case T_IDLE:
		if (e->type == SOCK_DGRAM)
			return e->rem || (waitfd(fd, POLLIN, 0) & POLLIN) ? T_DATA : 0;
		return e->qlen && (waitfd(fd, POLLIN, 0) & POLLIN) ? T_LISTEN : 0;
	case T_INCON:
		for (i = 0; i < e->npend; i++) {
			if (e->pend[i].dead) return T_DISCONNECT;
			if (peek(e->pend[i].sd) == -2) {
				e->pend[i].dead = 1;
				e->pend[i].reason = errno;
				return T_DISCONNECT;
			}
		}
		return waitfd(fd, POLLIN, 0) & POLLIN ? T_LISTEN : 0;
	case T_OUTCON:
		if (e->disc) return T_DISCONNECT;
		if (!(waitfd(fd, POLLOUT | POLLERR | POLLHUP, 0) > 0)) return 0;
		err = 0;
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &l);
		if (err) {
			e->disc = 1;
			e->reason = err;
			return T_DISCONNECT;
		}
		return T_CONNECT;
	}
	if (e->disc) return T_DISCONNECT;
	r = peek(fd);
	if (r == -2) {
		e->disc = 1;
		e->reason = errno;
		return T_DISCONNECT;
	}
	if (e->state != T_INREL) {
		if (r == 0) {
			e->fin = 1;
			return T_ORDREL;
		}
		if (r == 1) return T_DATA;
	}
	if (e->flow && (waitfd(fd, POLLOUT, 0) & POLLOUT)) {
		e->flow = 0;
		return T_GODATA;
	}
	return 0;
}

/* wait for the next event: the event, or -1 (errno) */
static int wait_event(struct ep *e)
{
	int ev;
	while (!(ev = event(e))) {
		if (waitfd(e->fd, e->state == T_OUTCON ? POLLOUT : POLLIN, -1) < 0) return -1;
	}
	return ev;
}

/* a new socket for the endpoint, with the same options; bound again to its
   address if REBIND; the old socket ends (abortively if ABORT) */
static int recycle(struct ep *e, int rebind, int abort)
{
	int fd = e->fd, s, fl, fdfl, ru = 0, one = 1;
	socklen_t l = sizeof ru;

	fl = fcntl(fd, F_GETFL);
	fdfl = fcntl(fd, F_GETFD);
	if ((s = socket(AF_INET, e->type, 0)) < 0) return -1;
	__xti_copyopts(fd, s);
	getsockopt(s, SOL_SOCKET, SO_REUSEADDR, &ru, &l);
	if (abort) {
		struct linger g = { 1, 0 };
		setsockopt(fd, SOL_SOCKET, SO_LINGER, &g, sizeof g);
	}
	if (dup2(s, fd) < 0) {
		close(s);
		return -1;
	}
	close(s);
	if (fl >= 0) fcntl(fd, F_SETFL, fl & O_NONBLOCK);
	if (fdfl >= 0) fcntl(fd, F_SETFD, fdfl);
	if (rebind) {
		struct sockaddr_in a = e->local;
		socklen_t al = sizeof a;
		int r;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		/* the port of an accepted connection is still a listener's: take another */
		r = bind(fd, (struct sockaddr *)&a, sizeof a);
		if (r < 0 && errno == EADDRINUSE) {
			a.sin_port = 0;
			r = bind(fd, (struct sockaddr *)&a, sizeof a);
		}
		if (r < 0) return -1;
		getsockname(fd, (struct sockaddr *)&e->local, &al);
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ru, sizeof ru);
	}
	e->fin = e->disc = e->flow = 0;
	free(e->rem);
	e->rem = 0;
	e->remlen = 0;
	return 0;
}

static void drop_pending(struct ep *e)
{
	while (e->npend > 0) close(e->pend[--e->npend].sd);
	free(e->pend);
	e->pend = 0;
	e->cap = 0;
}

static void remove_pending(struct ep *e, int i)
{
	memmove(&e->pend[i], &e->pend[i + 1], (e->npend - i - 1) * sizeof *e->pend);
	e->npend--;
}

static int find_pending(struct ep *e, int seq)
{
	int i;
	for (i = 0; i < e->npend; i++)
		if (e->pend[i].seq == seq) return i;
	return -1;
}

/* the address SIN in the netbuf NB; -1 with TBUFOVFLW if it does not fit */
static int put_addr(struct netbuf *nb, const struct sockaddr_in *sin)
{
	if (!nb) return 0;
	if (!nb->maxlen) {
		nb->len = 0;
		return 0;
	}
	if (nb->maxlen < SIN) return fail(TBUFOVFLW);
	memcpy(nb->buf, sin, SIN);
	nb->len = SIN;
	return 0;
}

static int get_addr(const struct netbuf *nb, struct sockaddr_in *sin)
{
	if (!nb || nb->len != SIN || !nb->buf) return -1;
	memcpy(sin, nb->buf, SIN);
	return sin->sin_family == AF_INET ? 0 : -1;
}

/* the options of an incoming option buffer, negotiated on socket FD */
static int apply_opts(int fd, int type, const struct netbuf *nb)
{
	unsigned char out[1024];
	unsigned outlen;
	int r;
	if (!nb || !nb->len) return 0;
	r = __xti_opts(fd, type, T_NEGOTIATE, nb->buf, nb->len, out, sizeof out, &outlen);
	if (r < 0) return -1;
	if (r != T_SUCCESS && r != T_PARTSUCCESS) return fail(TBADOPT);
	return 0;
}

int t_open(const char *name, int oflag, struct t_info *info)
{
	int type, fd;
	struct ep *e;

	if ((oflag & O_ACCMODE) != O_RDWR || (oflag & ~(O_ACCMODE | O_NONBLOCK | O_CLOEXEC)))
		return fail(TBADFLAG);
	if (name && !strcmp(name, "/dev/tcp")) type = SOCK_STREAM;
	else if (name && !strcmp(name, "/dev/udp")) type = SOCK_DGRAM;
	else return fail(TBADNAME);
	if ((fd = socket(AF_INET, type, 0)) < 0) return sysfail();
	if (oflag & O_NONBLOCK) fcntl(fd, F_SETFL, O_NONBLOCK);
	if (oflag & O_CLOEXEC) fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (!(e = reg(fd, type, T_UNBND))) {
		close(fd);
		t_errno = TSYSERR;
		errno = ENOMEM;
		return -1;
	}
	if (info) fill_info(type, info);
	return fd;
}

int t_close(int fd)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	pthread_mutex_lock(&tablock);
	tab[fd] = 0;
	pthread_mutex_unlock(&tablock);
	drop_pending(e);
	free(e->rem);
	free(e);
	if (close(fd) < 0) return sysfail();
	return 0;
}

int t_getinfo(int fd, struct t_info *info)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (info) fill_info(e->type, info);
	return 0;
}

int t_getstate(int fd)
{
	struct ep *e = get(fd);
	return e ? e->state : fail(TBADF);
}

int t_bind(int fd, const struct t_bind *req, struct t_bind *ret)
{
	struct ep *e = get(fd);
	struct sockaddr_in sin;
	socklen_t l = sizeof sin;
	unsigned qlen = 0;

	if (!e) return fail(TBADF);
	if (e->state != T_UNBND) return fail(TOUTSTATE);
	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	if (req && req->addr.len && get_addr(&req->addr, &sin) < 0) return fail(TBADADDR);
	if (req && e->type == SOCK_STREAM) qlen = req->qlen;
	if (bind(fd, (struct sockaddr *)&sin, sizeof sin) < 0) {
		switch (errno) {
		case EADDRINUSE: return fail(TADDRBUSY);
		case EACCES: return fail(TACCES);
		case EADDRNOTAVAIL: case EINVAL: case EAFNOSUPPORT: return fail(TBADADDR);
		}
		return sysfail();
	}
	if (qlen && listen(fd, qlen > SOMAXCONN ? SOMAXCONN : (int)qlen) < 0) {
		int er = errno;
		recycle(e, 0, 0);
		errno = er;
		return er == EADDRINUSE ? fail(TADDRBUSY) : sysfail();
	}
	getsockname(fd, (struct sockaddr *)&e->local, &l);
	e->qlen = qlen;
	e->state = T_IDLE;
	if (ret) {
		ret->qlen = qlen;
		if (put_addr(&ret->addr, &e->local) < 0) return -1;
	}
	return 0;
}

int t_unbind(int fd)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (e->state != T_IDLE) return fail(TOUTSTATE);
	if (recycle(e, 0, 0) < 0) return sysfail();
	e->qlen = 0;
	e->state = T_UNBND;
	return 0;
}

int t_getprotaddr(int fd, struct t_bind *bound, struct t_bind *peer)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (bound) {
		if (e->state == T_UNBND) bound->addr.len = 0;
		else if (put_addr(&bound->addr, &e->local) < 0) return -1;
	}
	if (peer) {
		if (e->state != T_DATAXFER && e->state != T_INREL && e->state != T_OUTCON && e->state != T_OUTREL)
			peer->addr.len = 0;
		else if (put_addr(&peer->addr, &e->peer) < 0) return -1;
	}
	return 0;
}

int t_look(int fd)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	return event(e);
}

int t_sync(int fd)
{
	struct ep *e = get(fd);
	int type, v = 0;
	socklen_t l = sizeof type;
	struct sockaddr_in sin;
	socklen_t sl = sizeof sin;

	if (e) return e->state;
	if (fcntl(fd, F_GETFD) < 0) return fail(TBADF);
	if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &l) < 0) return fail(TBADF);
	if ((type != SOCK_STREAM && type != SOCK_DGRAM) ||
	    getsockname(fd, (struct sockaddr *)&sin, &sl) < 0 || sin.sin_family != AF_INET)
		return fail(TBADF);
	if (!(e = reg(fd, type, T_UNBND))) {
		t_errno = TSYSERR;
		errno = ENOMEM;
		return -1;
	}
	e->local = sin;
	if (sin.sin_port || sin.sin_addr.s_addr) e->state = T_IDLE;
	if (type == SOCK_STREAM) {
		l = sizeof v;
		if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &v, &l) == 0 && v) {
			e->qlen = 1;
		} else {
			sl = sizeof sin;
			if (getpeername(fd, (struct sockaddr *)&sin, &sl) == 0) {
				e->peer = sin;
				e->state = T_DATAXFER;
			}
		}
	}
	return e->state;
}

int t_sysconf(int name)
{
	if (name == _SC_T_IOV_MAX) return T_IOV_MAX;
	return fail(TBADFLAG);
}

/* connection mode */

int t_listen(int fd, struct t_call *call)
{
	struct ep *e = get(fd);
	struct sockaddr_in from;
	socklen_t fl = sizeof from;
	int sd;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_IDLE && e->state != T_INCON) return fail(TOUTSTATE);
	if (!e->qlen) return fail(TBADQLEN);
	if (event(e) == T_DISCONNECT) return fail(TLOOK);
	if (e->npend >= (int)e->qlen) return fail(TQFULL);
	if (e->npend == e->cap) {
		struct pend *n = realloc(e->pend, (e->cap + 8) * sizeof *n);
		if (!n) {
			t_errno = TSYSERR;
			errno = ENOMEM;
			return -1;
		}
		e->pend = n;
		e->cap += 8;
	}
	if ((sd = accept(fd, (struct sockaddr *)&from, &fl)) < 0)
		return errno == EAGAIN || errno == EWOULDBLOCK ? fail(TNODATA) : sysfail();
	memset(&e->pend[e->npend], 0, sizeof e->pend[0]);
	e->pend[e->npend].sd = sd;
	e->pend[e->npend].seq = ++e->nextseq;
	e->pend[e->npend].from = from;
	e->npend++;
	e->state = T_INCON;
	if (call) {
		call->sequence = e->nextseq;
		call->opt.len = 0;
		call->udata.len = 0;
		if (put_addr(&call->addr, &from) < 0) return -1;
	}
	return 0;
}

int t_accept(int fd, int resfd, const struct t_call *call)
{
	struct ep *e = get(fd), *r = get(resfd);
	int i, sd, fl, fdfl;
	struct sockaddr_in loc;
	socklen_t l = sizeof loc;

	if (!e || !r) return fail(TBADF);
	if (e->type != r->type) return fail(TPROVMISMATCH);
	if (e->state != T_INCON) return fail(TOUTSTATE);
	if (resfd != fd) {
		if (r->state != T_IDLE && r->state != T_UNBND) return fail(TOUTSTATE);
		if (r->state == T_IDLE && r->qlen) return fail(TRESQLEN);
	} else if (e->npend > 1) {
		return fail(TINDOUT);
	}
	if (event(e) == T_DISCONNECT) return fail(TLOOK);
	if (!call || (i = find_pending(e, call->sequence)) < 0) return fail(TBADSEQ);
	if (call->udata.len) return fail(TBADDATA);
	sd = e->pend[i].sd;
	__xti_copyopts(resfd, sd);
	if (apply_opts(sd, e->type, &call->opt) < 0) return -1;
	fl = fcntl(resfd, F_GETFL);
	fdfl = fcntl(resfd, F_GETFD);
	r->peer = e->pend[i].from;
	remove_pending(e, i);
	if (dup2(sd, resfd) < 0) return sysfail();
	close(sd);
	if (fl >= 0) fcntl(resfd, F_SETFL, fl & O_NONBLOCK);
	if (fdfl >= 0) fcntl(resfd, F_SETFD, fdfl);
	getsockname(resfd, (struct sockaddr *)&loc, &l);
	r->local = loc;
	r->qlen = 0;
	r->fin = r->disc = r->flow = 0;
	r->state = T_DATAXFER;
	if (resfd != fd) e->state = e->npend ? T_INCON : T_IDLE;
	return 0;
}

int t_connect(int fd, const struct t_call *snd, struct t_call *rcv)
{
	struct ep *e = get(fd);
	struct sockaddr_in sin;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_IDLE || e->qlen) return fail(TOUTSTATE);
	if (!snd || get_addr(&snd->addr, &sin) < 0) return fail(TBADADDR);
	if (snd->udata.len) return fail(TBADDATA);
	if (apply_opts(fd, e->type, &snd->opt) < 0) return -1;
	e->peer = sin;
	e->fin = e->disc = e->flow = 0;
	if (connect(fd, (struct sockaddr *)&sin, sizeof sin) < 0) {
		int er = errno;
		if (er == EINPROGRESS) {
			e->state = T_OUTCON;
			return fail(TNODATA);
		}
		if (er == EINTR) {
			e->state = T_OUTCON;
			return sysfail();
		}
		if (is_disc(er)) {
			e->state = T_OUTCON;
			e->disc = 1;
			e->reason = er;
			return fail(TLOOK);
		}
		switch (er) {
		case EADDRINUSE: return fail(TADDRBUSY);
		case EACCES: case EPERM: return fail(TACCES);
		case EAFNOSUPPORT: case EADDRNOTAVAIL: case EINVAL: return fail(TBADADDR);
		case EISCONN: return fail(TOUTSTATE);
		}
		return sysfail();
	}
	e->state = T_DATAXFER;
	if (rcv) {
		rcv->opt.len = 0;
		rcv->udata.len = 0;
		if (put_addr(&rcv->addr, &sin) < 0) return -1;
	}
	return 0;
}

int t_rcvconnect(int fd, struct t_call *call)
{
	struct ep *e = get(fd);
	int err = 0, r;
	socklen_t l = sizeof err;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_OUTCON) return fail(TOUTSTATE);
	if (e->disc) return fail(TLOOK);
	r = waitfd(fd, POLLOUT | POLLERR | POLLHUP, nonblock(fd) ? 0 : -1);
	if (r < 0) return sysfail();
	if (!r) return fail(TNODATA);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &l);
	if (err) {
		e->disc = 1;
		e->reason = err;
		return fail(TLOOK);
	}
	e->state = T_DATAXFER;
	if (call) {
		call->opt.len = 0;
		call->udata.len = 0;
		if (put_addr(&call->addr, &e->peer) < 0) return -1;
	}
	return 0;
}

static int valid_iov(unsigned cnt)
{
	return cnt >= 1 && cnt <= T_IOV_MAX;
}

static int send_common(struct ep *e, struct iovec *iov, unsigned cnt, int flags)
{
	struct msghdr m;
	ssize_t n;

	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_DATAXFER && e->state != T_INREL) return fail(TOUTSTATE);
	if (flags & ~(T_MORE | T_EXPEDITED | T_PUSH) || flags & T_EXPEDITED) return fail(TBADFLAG);
	if (e->disc) return fail(TLOOK);
	memset(&m, 0, sizeof m);
	m.msg_iov = iov;
	m.msg_iovlen = cnt;
	n = sendmsg(e->fd, &m, MSG_NOSIGNAL);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			e->flow = 1;
			return fail(TFLOW);
		}
		if (is_disc(errno)) {
			e->disc = 1;
			e->reason = errno;
			return fail(TLOOK);
		}
		return sysfail();
	}
	return (int)n;
}

int t_snd(int fd, void *buf, unsigned nbytes, int flags)
{
	struct ep *e = get(fd);
	struct iovec v;
	if (!e) return fail(TBADF);
	if (!nbytes) return e->type == SOCK_STREAM ? fail(TBADDATA) : fail(TNOTSUPPORT);
	v.iov_base = buf;
	v.iov_len = nbytes;
	return send_common(e, &v, 1, flags);
}

int t_sndv(int fd, const struct t_iovec *iov, unsigned cnt, int flags)
{
	struct ep *e = get(fd);
	unsigned i;
	size_t total = 0;
	if (!e) return fail(TBADF);
	if (!valid_iov(cnt)) return fail(TBADDATA);
	for (i = 0; i < cnt; i++) total += iov[i].iov_len;
	if (!total) return fail(TBADDATA);
	return send_common(e, (struct iovec *)iov, cnt, flags);
}

static int rcv_common(struct ep *e, struct iovec *iov, unsigned cnt, int *flags)
{
	struct msghdr m;
	ssize_t n;

	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_DATAXFER && e->state != T_OUTREL) return fail(TOUTSTATE);
	if (flags) *flags = 0;
	if (e->disc) return fail(TLOOK);
	memset(&m, 0, sizeof m);
	m.msg_iov = iov;
	m.msg_iovlen = cnt;
	n = recvmsg(e->fd, &m, 0);
	if (n > 0) return (int)n;
	if (n == 0) {
		e->fin = 1;
		return fail(TLOOK);
	}
	if (errno == EAGAIN || errno == EWOULDBLOCK) return fail(TNODATA);
	if (is_disc(errno)) {
		e->disc = 1;
		e->reason = errno;
		return fail(TLOOK);
	}
	return sysfail();
}

int t_rcv(int fd, void *buf, unsigned nbytes, int *flags)
{
	struct ep *e = get(fd);
	struct iovec v;
	if (!e) return fail(TBADF);
	v.iov_base = buf;
	v.iov_len = nbytes;
	return rcv_common(e, &v, 1, flags);
}

int t_rcvv(int fd, struct t_iovec *iov, unsigned cnt, int *flags)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (!valid_iov(cnt)) return fail(TBADDATA);
	return rcv_common(e, (struct iovec *)iov, cnt, flags);
}

int t_snddis(int fd, const struct t_call *call)
{
	struct ep *e = get(fd);
	int i;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	switch (e->state) {
	case T_INCON:
		if (!call || (i = find_pending(e, call->sequence)) < 0) return fail(TBADSEQ);
		{
			struct linger g = { 1, 0 };
			setsockopt(e->pend[i].sd, SOL_SOCKET, SO_LINGER, &g, sizeof g);
		}
		close(e->pend[i].sd);
		remove_pending(e, i);
		e->state = e->npend ? T_INCON : T_IDLE;
		return 0;
	case T_DATAXFER: case T_OUTCON: case T_OUTREL: case T_INREL:
		break;
	default:
		return fail(TOUTSTATE);
	}
	if (call && call->udata.len) return fail(TBADDATA);
	if (recycle(e, 1, 1) < 0) return sysfail();
	e->state = T_IDLE;
	return 0;
}

int t_rcvdis(int fd, struct t_discon *d)
{
	struct ep *e = get(fd);
	int i, ev, seq = 0, reason;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	switch (e->state) {
	case T_INCON: case T_DATAXFER: case T_OUTCON: case T_OUTREL: case T_INREL:
		break;
	default:
		return fail(TOUTSTATE);
	}
	ev = event(e);
	if (!ev && !nonblock(fd)) {
		if ((ev = wait_event(e)) < 0) return sysfail();
	}
	if (e->state == T_INCON) {
		for (i = 0; i < e->npend && !e->pend[i].dead; i++)
			;
		if (i == e->npend) return ev && !nonblock(fd) ? fail(TLOOK) : fail(TNODATA);
		reason = e->pend[i].reason;
		seq = e->pend[i].seq;
		close(e->pend[i].sd);
		remove_pending(e, i);
		e->state = e->npend ? T_INCON : T_IDLE;
	} else {
		if (!e->disc) return ev && !nonblock(fd) ? fail(TLOOK) : fail(TNODATA);
		reason = e->reason;
		if (recycle(e, 1, 1) < 0) return sysfail();
		e->state = T_IDLE;
	}
	if (d) {
		d->reason = reason;
		d->sequence = seq;
		d->udata.len = 0;
	}
	return 0;
}

int t_sndrel(int fd)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_DATAXFER && e->state != T_INREL) return fail(TOUTSTATE);
	if (e->disc) return fail(TLOOK);
	if (e->state == T_DATAXFER) {
		if (shutdown(fd, SHUT_WR) < 0) return sysfail();
		e->state = T_OUTREL;
		return 0;
	}
	if (recycle(e, 1, 0) < 0) return sysfail();
	e->state = T_IDLE;
	return 0;
}

int t_sndreldata(int fd, struct t_discon *d)
{
	if (d && d->udata.len) return fail(TBADDATA);
	return t_sndrel(fd);
}

int t_rcvrel(int fd)
{
	struct ep *e = get(fd);
	int ev;

	if (!e) return fail(TBADF);
	if (e->type != SOCK_STREAM) return fail(TNOTSUPPORT);
	if (e->state != T_DATAXFER && e->state != T_OUTREL) return fail(TOUTSTATE);
	ev = event(e);
	if (!ev && !nonblock(fd)) {
		if ((ev = wait_event(e)) < 0) return sysfail();
	}
	if (ev == T_DISCONNECT || ev == T_DATA) return fail(TLOOK);
	if (ev != T_ORDREL) return fail(TNODATA);
	e->fin = 0;
	if (e->state == T_DATAXFER) {
		e->state = T_INREL;
		return 0;
	}
	if (recycle(e, 1, 0) < 0) return sysfail();
	e->state = T_IDLE;
	return 0;
}

int t_rcvreldata(int fd, struct t_discon *d)
{
	int r = t_rcvrel(fd);
	if (r == 0 && d) {
		d->udata.len = 0;
		d->reason = 0;
		d->sequence = 0;
	}
	return r;
}

/* connectionless mode */

static int sendud(struct ep *e, const struct netbuf *addr, const struct netbuf *opt, struct iovec *iov, unsigned cnt)
{
	struct sockaddr_in sin;
	struct msghdr m;
	size_t total = 0;
	unsigned i;
	ssize_t n;

	if (e->type != SOCK_DGRAM) return fail(TNOTSUPPORT);
	if (e->state != T_IDLE) return fail(TOUTSTATE);
	if (get_addr(addr, &sin) < 0) return fail(TBADADDR);
	for (i = 0; i < cnt; i++) total += iov[i].iov_len;
	if (total > MAXUDP) return fail(TBADDATA);
	if (apply_opts(e->fd, e->type, opt) < 0) return -1;
	memset(&m, 0, sizeof m);
	m.msg_name = &sin;
	m.msg_namelen = sizeof sin;
	m.msg_iov = iov;
	m.msg_iovlen = cnt;
	n = sendmsg(e->fd, &m, MSG_NOSIGNAL);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			e->flow = 1;
			return fail(TFLOW);
		}
		if (errno == EMSGSIZE) return fail(TBADDATA);
		return sysfail();
	}
	return 0;
}

int t_sndudata(int fd, const struct t_unitdata *ud)
{
	struct ep *e = get(fd);
	struct iovec v;
	if (!e) return fail(TBADF);
	v.iov_base = ud->udata.buf;
	v.iov_len = ud->udata.len;
	return sendud(e, &ud->addr, &ud->opt, &v, 1);
}

int t_sndvudata(int fd, struct t_unitdata *ud, struct t_iovec *iov, unsigned cnt)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (!valid_iov(cnt)) return fail(TBADDATA);
	return sendud(e, &ud->addr, &ud->opt, (struct iovec *)iov, cnt);
}

/* a data unit into IOV; the rest of one that did not fit is kept for the
   next call, which answers with no address; returns the bytes, or -1 */
static int rcvud(struct ep *e, struct netbuf *addr, struct netbuf *opt, struct iovec *iov, unsigned cnt, int *flags)
{
	unsigned char *buf = 0, *data;
	size_t n, copied = 0, c;
	unsigned i;
	int cont = e->rem != 0;
	struct sockaddr_in from;
	socklen_t fl = sizeof from;
	ssize_t r;

	if (e->type != SOCK_DGRAM) return fail(TNOTSUPPORT);
	if (e->state != T_IDLE) return fail(TOUTSTATE);
	if (flags) *flags = 0;
	if (cont) {
		data = e->rem;
		n = e->remlen;
		if (addr) addr->len = 0;
	} else {
		if (!(buf = malloc(65536))) {
			t_errno = TSYSERR;
			errno = ENOMEM;
			return -1;
		}
		r = recvfrom(e->fd, buf, 65536, 0, (struct sockaddr *)&from, &fl);
		if (r < 0) {
			free(buf);
			if (errno == EAGAIN || errno == EWOULDBLOCK) return fail(TNODATA);
			return sysfail();
		}
		n = (size_t)r;
		data = buf;
		if (addr && put_addr(addr, &from) < 0) {
			free(buf);
			return -1;
		}
	}
	if (opt) opt->len = 0;
	for (i = 0; i < cnt && copied < n; i++) {
		c = n - copied < iov[i].iov_len ? n - copied : iov[i].iov_len;
		memcpy(iov[i].iov_base, data + copied, c);
		copied += c;
	}
	if (copied < n) {
		unsigned char *rest = malloc(n - copied);
		if (rest) memcpy(rest, data + copied, n - copied);
		free(e->rem);
		e->rem = rest;
		e->remlen = rest ? n - copied : 0;
		if (flags && rest) *flags = T_MORE;
	} else {
		free(e->rem);
		e->rem = 0;
		e->remlen = 0;
	}
	free(buf);
	return (int)copied;
}

int t_rcvudata(int fd, struct t_unitdata *ud, int *flags)
{
	struct ep *e = get(fd);
	struct iovec v;
	int n;
	if (!e) return fail(TBADF);
	v.iov_base = ud->udata.buf;
	v.iov_len = ud->udata.maxlen;
	if ((n = rcvud(e, &ud->addr, &ud->opt, &v, 1, flags)) < 0) return -1;
	ud->udata.len = n;
	return 0;
}

int t_rcvvudata(int fd, struct t_unitdata *ud, struct t_iovec *iov, unsigned cnt, int *flags)
{
	struct ep *e = get(fd);
	if (!e) return fail(TBADF);
	if (!valid_iov(cnt)) return fail(TBADDATA);
	return rcvud(e, &ud->addr, &ud->opt, (struct iovec *)iov, cnt, flags);
}

int t_rcvuderr(int fd, struct t_uderr *uderr)
{
	struct ep *e = get(fd);
	(void)uderr;
	if (!e) return fail(TBADF);
	if (e->type != SOCK_DGRAM) return fail(TNOTSUPPORT);
	if (e->state != T_IDLE) return fail(TOUTSTATE);
	return fail(TNOUDERR);
}

int t_optmgmt(int fd, const struct t_optmgmt *req, struct t_optmgmt *ret)
{
	struct ep *e = get(fd);
	unsigned char *out;
	unsigned outlen = 0, outmax = ret ? ret->opt.maxlen : 0;
	int r;

	if (!e) return fail(TBADF);
	if (!req) return fail(TBADOPT);
	out = outmax ? ret->opt.buf : (unsigned char *)&outlen;
	r = __xti_opts(fd, e->type, req->flags, req->opt.buf, req->opt.len, out, outmax, &outlen);
	if (r < 0) return -1;
	if (ret) {
		ret->opt.len = outlen;
		ret->flags = r;
	}
	return 0;
}

/* t_alloc and t_free */

static int field_size(const struct t_info *i, int struct_type, int field)
{
	switch (field) {
	case T_ADDR: return i->addr;
	case T_OPT: return i->options;
	}
	switch (struct_type) {
	case T_CALL: return i->connect;
	case T_DIS: return i->discon;
	default: return i->tsdu;
	}
}

static int alloc_nb(struct netbuf *nb, int size, int infinite_ok)
{
	nb->len = 0;
	nb->maxlen = 0;
	nb->buf = 0;
	if (size == T_INFINITE) size = infinite_ok ? 65536 : 0;
	if (size <= 0) return 0;
	if (!(nb->buf = malloc(size))) return -1;
	nb->maxlen = size;
	return 0;
}

void *t_alloc(int fd, int struct_type, int fields)
{
	struct ep *e = 0;
	struct t_info info;
	size_t sz;
	void *p;
	struct netbuf *nb[3] = { 0, 0, 0 };
	int flds[3] = { T_ADDR, T_OPT, T_UDATA };
	int i, size;

	if (struct_type != T_INFO && !(e = get(fd))) {
		t_errno = TBADF;
		return 0;
	}
	if (e) fill_info(e->type, &info);
	else memset(&info, 0, sizeof info);
	switch (struct_type) {
	case T_BIND: sz = sizeof(struct t_bind); break;
	case T_OPTMGMT: sz = sizeof(struct t_optmgmt); break;
	case T_CALL: sz = sizeof(struct t_call); break;
	case T_DIS: sz = sizeof(struct t_discon); break;
	case T_UNITDATA: sz = sizeof(struct t_unitdata); break;
	case T_UDERROR: sz = sizeof(struct t_uderr); break;
	case T_INFO: sz = sizeof(struct t_info); break;
	default: t_errno = TNOSTRUCTYPE; return 0;
	}
	if (e && (((struct_type == T_CALL || struct_type == T_DIS) && e->type != SOCK_STREAM) ||
	    ((struct_type == T_UNITDATA || struct_type == T_UDERROR) && e->type != SOCK_DGRAM))) {
		t_errno = TNOSTRUCTYPE;
		return 0;
	}
	if (!(p = calloc(1, sz))) {
		t_errno = TSYSERR;
		errno = ENOMEM;
		return 0;
	}
	switch (struct_type) {
	case T_BIND: nb[0] = &((struct t_bind *)p)->addr; break;
	case T_OPTMGMT: nb[1] = &((struct t_optmgmt *)p)->opt; break;
	case T_CALL: nb[0] = &((struct t_call *)p)->addr; nb[1] = &((struct t_call *)p)->opt; nb[2] = &((struct t_call *)p)->udata; break;
	case T_DIS: nb[2] = &((struct t_discon *)p)->udata; break;
	case T_UNITDATA: nb[0] = &((struct t_unitdata *)p)->addr; nb[1] = &((struct t_unitdata *)p)->opt; nb[2] = &((struct t_unitdata *)p)->udata; break;
	case T_UDERROR: nb[0] = &((struct t_uderr *)p)->addr; nb[1] = &((struct t_uderr *)p)->opt; break;
	}
	for (i = 0; i < 3; i++) {
		if (!nb[i] || !(fields & flds[i])) continue;
		size = field_size(&info, struct_type, flds[i]);
		if (size == T_INVALID) {
			if ((fields & T_ALL) == T_ALL) continue;
			t_free(p, struct_type);
			t_errno = TSYSERR;
			errno = EINVAL;
			return 0;
		}
		if (alloc_nb(nb[i], size, 1) < 0) {
			t_free(p, struct_type);
			t_errno = TSYSERR;
			errno = ENOMEM;
			return 0;
		}
	}
	return p;
}

int t_free(void *ptr, int struct_type)
{
	struct netbuf *nb[3] = { 0, 0, 0 };
	int i;

	if (!ptr) return 0;
	switch (struct_type) {
	case T_BIND: nb[0] = &((struct t_bind *)ptr)->addr; break;
	case T_OPTMGMT: nb[1] = &((struct t_optmgmt *)ptr)->opt; break;
	case T_CALL: nb[0] = &((struct t_call *)ptr)->addr; nb[1] = &((struct t_call *)ptr)->opt; nb[2] = &((struct t_call *)ptr)->udata; break;
	case T_DIS: nb[2] = &((struct t_discon *)ptr)->udata; break;
	case T_UNITDATA: nb[0] = &((struct t_unitdata *)ptr)->addr; nb[1] = &((struct t_unitdata *)ptr)->opt; nb[2] = &((struct t_unitdata *)ptr)->udata; break;
	case T_UDERROR: nb[0] = &((struct t_uderr *)ptr)->addr; nb[1] = &((struct t_uderr *)ptr)->opt; break;
	case T_INFO: break;
	default: return fail(TNOSTRUCTYPE);
	}
	for (i = 0; i < 3; i++)
		if (nb[i]) free(nb[i]->buf);
	free(ptr);
	return 0;
}
