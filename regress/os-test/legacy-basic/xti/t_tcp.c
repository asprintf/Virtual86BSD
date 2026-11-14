/*[SUSv2]*/
/* Test whether a connection goes through bind, connect, listen, accept, data, orderly release and a new connection on the endpoints. */

#include "../xti_util.h"

int main(void)
{
	struct sockaddr_in saddr, caddr, from;
	struct t_call call, lcall;
	char buf[100];
	int srv, cli, acc, flags;

	srv = endpoint("/dev/tcp", 0, 2, &saddr);
	CHECK(t_getstate(srv) == T_IDLE);
	cli = endpoint("/dev/tcp", 0, 0, &caddr);
	FAILS(t_listen(cli, NULL), TBADQLEN);
	callto(&call, &saddr);
	CHECK(t_connect(cli, &call, NULL) == 0);
	CHECK(t_getstate(cli) == T_DATAXFER);
	FAILS(t_connect(cli, &call, NULL), TOUTSTATE);

	expect_event(srv, T_LISTEN);
	memset(&lcall, 0, sizeof(lcall));
	lcall.addr.maxlen = sizeof(from);
	lcall.addr.buf = &from;
	CHECK(t_listen(srv, &lcall) == 0);
	CHECK(t_getstate(srv) == T_INCON);
	CHECK(lcall.sequence != 0 && lcall.addr.len == sizeof(from));
	CHECK(from.sin_port == caddr.sin_port);

	acc = t_open("/dev/tcp", O_RDWR, NULL);
	CHECK(acc >= 0);
	lcall.sequence += 100;
	FAILS(t_accept(srv, acc, &lcall), TBADSEQ);
	lcall.sequence -= 100;
	CHECK(t_accept(srv, acc, &lcall) == 0);
	CHECK(t_getstate(acc) == T_DATAXFER && t_getstate(srv) == T_IDLE);

	CHECK(t_snd(cli, "hello", 5, 0) == 5);
	CHECK(t_rcv(acc, buf, sizeof(buf), &flags) == 5 && memcmp(buf, "hello", 5) == 0);
	CHECK(t_snd(acc, "world", 5, 0) == 5);
	CHECK(t_rcv(cli, buf, sizeof(buf), &flags) == 5 && memcmp(buf, "world", 5) == 0);
	CHECK(t_look(cli) == 0 && t_look(acc) == 0);

	{
		struct sockaddr_in bound, peer;
		struct t_bind b, p;
		memset(&b, 0, sizeof(b)); memset(&p, 0, sizeof(p));
		b.addr.maxlen = p.addr.maxlen = sizeof(bound);
		b.addr.buf = &bound; p.addr.buf = &peer;
		CHECK(t_getprotaddr(cli, &b, &p) == 0);
		CHECK(b.addr.len == sizeof(bound) && bound.sin_port == caddr.sin_port);
		CHECK(p.addr.len == sizeof(peer) && peer.sin_port == saddr.sin_port);
	}

	/* orderly release, the client first */
	CHECK(t_sndrel(cli) == 0);
	CHECK(t_getstate(cli) == T_OUTREL);
	FAILS(t_snd(cli, "x", 1, 0), TOUTSTATE);
	expect_event(acc, T_ORDREL);
	FAILS(t_rcv(acc, buf, sizeof(buf), &flags), TLOOK);
	CHECK(t_rcvrel(acc) == 0);
	CHECK(t_getstate(acc) == T_INREL);
	FAILS(t_rcv(acc, buf, sizeof(buf), &flags), TOUTSTATE);
	CHECK(t_snd(acc, "x", 1, 0) == 1);
	CHECK(t_rcv(cli, buf, sizeof(buf), &flags) == 1);
	CHECK(t_sndrel(acc) == 0);
	CHECK(t_getstate(acc) == T_IDLE);
	expect_event(cli, T_ORDREL);
	CHECK(t_rcvrel(cli) == 0);
	CHECK(t_getstate(cli) == T_IDLE);

	/* the endpoints again, accepting on the listening endpoint */
	CHECK(t_connect(cli, &call, NULL) == 0);
	expect_event(srv, T_LISTEN);
	CHECK(t_listen(srv, &lcall) == 0);
	CHECK(t_accept(srv, srv, &lcall) == 0);
	CHECK(t_getstate(srv) == T_DATAXFER);
	CHECK(t_snd(cli, "again", 5, 0) == 5);
	CHECK(t_rcv(srv, buf, sizeof(buf), &flags) == 5);

	/* the vectors */
	{
		struct t_iovec out[2], in[2];
		char a[4], b[8];
		out[0].iov_base = "ab"; out[0].iov_len = 2;
		out[1].iov_base = "cdef"; out[1].iov_len = 4;
		CHECK(t_sndv(cli, out, 2, 0) == 6);
		in[0].iov_base = a; in[0].iov_len = 3;
		in[1].iov_base = b; in[1].iov_len = sizeof(b);
		expect_event(srv, T_DATA);
		CHECK(t_rcvv(srv, in, 2, &flags) == 6);
		CHECK(memcmp(a, "abc", 3) == 0 && memcmp(b, "def", 3) == 0);
	}

	CHECK(t_close(cli) == 0 && t_close(srv) == 0 && t_close(acc) == 0);
	return 0;
}
