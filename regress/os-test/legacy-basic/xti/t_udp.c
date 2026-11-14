/*[SUSv2]*/
/* Test whether data units go between connectionless endpoints, with the rest of a long one, with vectors, and whether the connection-mode functions fail. */

#include "../xti_util.h"

int main(void)
{
	struct sockaddr_in aaddr, baddr, from;
	struct t_unitdata ud;
	struct t_uderr uderr;
	struct t_call call;
	char buf[32];
	int a, b, flags;
	struct t_iovec out[2], in[2];
	char x[4], y[16];

	a = endpoint("/dev/udp", 0, 0, &aaddr);
	b = endpoint("/dev/udp", 1, 0, &baddr);
	memset(&ud, 0, sizeof(ud));
	ud.addr.maxlen = sizeof(from);
	ud.udata.maxlen = sizeof(buf);
	ud.udata.buf = buf;
	ud.addr.buf = &from;
	FAILS(t_rcvudata(b, &ud, &flags), TNODATA);
	CHECK(t_look(b) == 0);

	ud.addr.len = sizeof(baddr);
	ud.addr.buf = &baddr;
	ud.udata.len = 8;
	ud.udata.buf = "datagram";
	CHECK(t_sndudata(a, &ud) == 0);
	expect_event(b, T_DATA);
	ud.addr.maxlen = sizeof(from);
	ud.addr.buf = &from;
	ud.udata.maxlen = sizeof(buf);
	ud.udata.buf = buf;
	flags = 99;
	CHECK(t_rcvudata(b, &ud, &flags) == 0);
	CHECK(flags == 0 && ud.udata.len == 8 && memcmp(buf, "datagram", 8) == 0);
	CHECK(ud.addr.len == sizeof(from) && from.sin_port == aaddr.sin_port);

	/* a data unit longer than the buffer: T_MORE, then the rest */
	ud.addr.len = sizeof(baddr);
	ud.addr.buf = &baddr;
	ud.udata.len = 10;
	ud.udata.buf = "0123456789";
	CHECK(t_sndudata(a, &ud) == 0);
	expect_event(b, T_DATA);
	ud.addr.maxlen = sizeof(from);
	ud.addr.buf = &from;
	ud.udata.maxlen = 4;
	ud.udata.buf = buf;
	CHECK(t_rcvudata(b, &ud, &flags) == 0);
	CHECK(flags == T_MORE && ud.udata.len == 4 && memcmp(buf, "0123", 4) == 0 && ud.addr.len == sizeof(from));
	ud.udata.maxlen = 32;
	CHECK(t_rcvudata(b, &ud, &flags) == 0);
	CHECK(flags == 0 && ud.udata.len == 6 && memcmp(buf, "456789", 6) == 0 && ud.addr.len == 0);

	/* an empty one, and the vectors */
	ud.addr.len = sizeof(baddr);
	ud.addr.buf = &baddr;
	ud.udata.len = 0;
	CHECK(t_sndudata(a, &ud) == 0);
	expect_event(b, T_DATA);
	ud.addr.maxlen = sizeof(from);
	ud.addr.buf = &from;
	ud.udata.maxlen = sizeof(buf);
	ud.udata.buf = buf;
	CHECK(t_rcvudata(b, &ud, &flags) == 0 && ud.udata.len == 0);
	out[0].iov_base = "ab"; out[0].iov_len = 2;
	out[1].iov_base = "cdef"; out[1].iov_len = 4;
	ud.addr.len = sizeof(baddr);
	ud.addr.buf = &baddr;
	CHECK(t_sndvudata(a, &ud, out, 2) == 0);
	expect_event(b, T_DATA);
	in[0].iov_base = x; in[0].iov_len = 3;
	in[1].iov_base = y; in[1].iov_len = sizeof(y);
	ud.addr.maxlen = sizeof(from);
	ud.addr.buf = &from;
	CHECK(t_rcvvudata(b, &ud, in, 2, &flags) == 6);
	CHECK(memcmp(x, "abc", 3) == 0 && memcmp(y, "def", 3) == 0 && flags == 0);

	/* too small an address buffer */
	ud.addr.len = sizeof(baddr);
	ud.addr.buf = &baddr;
	ud.udata.len = 1;
	ud.udata.buf = "z";
	CHECK(t_sndudata(a, &ud) == 0);
	expect_event(b, T_DATA);
	ud.addr.maxlen = 4;
	ud.addr.buf = &from;
	ud.udata.maxlen = sizeof(buf);
	ud.udata.buf = buf;
	FAILS(t_rcvudata(b, &ud, &flags), TBUFOVFLW);

	/* the errors */
	ud.addr.len = 3;
	FAILS(t_sndudata(a, &ud), TBADADDR);
	FAILS(t_rcvuderr(a, &uderr), TNOUDERR);
	callto(&call, &baddr);
	FAILS(t_connect(a, &call, NULL), TNOTSUPPORT);
	FAILS(t_listen(a, &call), TNOTSUPPORT);
	FAILS(t_snd(a, "x", 1, 0), TNOTSUPPORT);
	CHECK(t_close(a) == 0 && t_close(b) == 0);
	return 0;
}
