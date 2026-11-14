/*[SUSv2]*/
/* Test whether O_NONBLOCK endpoints fail with TNODATA until the event comes, and whether t_sync takes an endpoint made without t_open. */

#include <sys/socket.h>

#include "../xti_util.h"

int main(void)
{
	struct sockaddr_in saddr, caddr;
	struct t_call call, lcall;
	char buf[10];
	int srv, cli, flags;

	srv = endpoint("/dev/tcp", 1, 2, &saddr);
	cli = endpoint("/dev/tcp", 1, 0, &caddr);
	FAILS(t_listen(srv, NULL), TNODATA);
	callto(&call, &saddr);
	if ( t_connect(cli, &call, NULL) == 0 )
	{
		CHECK(t_getstate(cli) == T_DATAXFER);
	}
	else
	{
		CHECK(t_errno == TNODATA && t_getstate(cli) == T_OUTCON);
		FAILS(t_connect(cli, &call, NULL), TOUTSTATE);
		expect_event(cli, T_CONNECT);
		CHECK(t_rcvconnect(cli, NULL) == 0);
		CHECK(t_getstate(cli) == T_DATAXFER);
	}
	FAILS(t_rcv(cli, buf, sizeof(buf), &flags), TNODATA);
	expect_event(srv, T_LISTEN);
	memset(&lcall, 0, sizeof(lcall));
	CHECK(t_listen(srv, &lcall) == 0);
	CHECK(t_accept(srv, srv, &lcall) == 0);
	FAILS(t_rcv(srv, buf, sizeof(buf), &flags), TNODATA);

	/* a flow control restriction, and its end */
	{
		char big[65536];
		int n, total = 0, blocked = 0;
		memset(big, 'x', sizeof(big));
		for ( n = 0; n < 1000 && !blocked; n++ )
		{
			int r = t_snd(cli, big, sizeof(big), 0);
			if ( r < 0 && t_errno == TFLOW )
				blocked = 1;
			else if ( r < 0 )
				errx(1, "t_snd failed with t_errno %d", t_errno);
			else
				total += r;
		}
		CHECK(blocked);
		while ( total > 0 )
		{
			int r = t_rcv(srv, big, sizeof(big), &flags);
			if ( r < 0 )
				break;
			total -= r;
		}
		expect_event(cli, T_GODATA);
	}

	/* endpoints t_open did not make */
	{
		struct sockaddr_in a;
		int s1 = socket(AF_INET, SOCK_STREAM, 0);
		int s2 = socket(AF_INET, SOCK_STREAM, 0);
		int s3 = socket(AF_INET, SOCK_STREAM, 0);
		CHECK(s1 >= 0 && s2 >= 0 && s3 >= 0);
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		CHECK(t_sync(s1) == T_UNBND);
		CHECK(bind(s2, (struct sockaddr*) &a, sizeof(a)) == 0);
		CHECK(t_sync(s2) == T_IDLE);
		struct sockaddr_in la = a;
		socklen_t ll = sizeof(la);
		int l = socket(AF_INET, SOCK_STREAM, 0);
		CHECK(l >= 0 && bind(l, (struct sockaddr*) &la, sizeof(la)) == 0 && listen(l, 1) == 0);
		CHECK(getsockname(l, (struct sockaddr*) &la, &ll) == 0);
		CHECK(bind(s3, (struct sockaddr*) &a, sizeof(a)) == 0);
		CHECK(connect(s3, (struct sockaddr*) &la, sizeof(la)) == 0);
		CHECK(t_sync(s3) == T_DATAXFER);
		CHECK(t_snd(s3, "x", 1, 0) == 1);
		FAILS(t_sync(-1), TBADF);
	}
	return 0;
}
