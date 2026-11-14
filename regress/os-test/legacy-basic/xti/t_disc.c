/*[SUSv2]*/
/* Test whether an abortive release, a rejected connection and a refused connection give disconnect indications. */

#include "../xti_util.h"

int main(void)
{
	struct sockaddr_in saddr, caddr, other;
	struct t_call call, lcall;
	struct t_discon dis;
	char buf[10];
	int srv, cli, flags, other_fd, i;

	srv = endpoint("/dev/tcp", 0, 4, &saddr);
	cli = endpoint("/dev/tcp", 0, 0, &caddr);
	callto(&call, &saddr);

	/* the server rejects the connection */
	CHECK(t_connect(cli, &call, NULL) == 0);
	expect_event(srv, T_LISTEN);
	memset(&lcall, 0, sizeof(lcall));
	CHECK(t_listen(srv, &lcall) == 0);
	CHECK(t_getstate(srv) == T_INCON);
	FAILS(t_snddis(srv, NULL), TBADSEQ);
	CHECK(t_snddis(srv, &lcall) == 0);
	CHECK(t_getstate(srv) == T_IDLE);
	expect_event(cli, T_DISCONNECT);
	FAILS(t_rcv(cli, buf, sizeof(buf), &flags), TLOOK);
	memset(&dis, 0, sizeof(dis));
	CHECK(t_rcvdis(cli, &dis) == 0);
	CHECK(dis.reason != 0);
	CHECK(t_getstate(cli) == T_IDLE);
	FAILS(t_rcvdis(cli, &dis), TOUTSTATE);

	/* the client breaks an established connection */
	CHECK(t_connect(cli, &call, NULL) == 0);
	expect_event(srv, T_LISTEN);
	CHECK(t_listen(srv, &lcall) == 0);
	i = t_open("/dev/tcp", O_RDWR, NULL);
	CHECK(t_accept(srv, i, &lcall) == 0);
	CHECK(t_snddis(cli, NULL) == 0);
	CHECK(t_getstate(cli) == T_IDLE);
	expect_event(i, T_DISCONNECT);
	FAILS(t_snd(i, "x", 1, 0), TLOOK);
	CHECK(t_rcvdis(i, NULL) == 0);
	CHECK(t_getstate(i) == T_IDLE);

	/* a connection to a port nobody listens on */
	other_fd = endpoint("/dev/tcp", 0, 0, &other);
	CHECK(t_close(other_fd) == 0);
	callto(&call, &other);
	FAILS(t_connect(cli, &call, NULL), TLOOK);
	CHECK(t_look(cli) == T_DISCONNECT);
	CHECK(t_rcvdis(cli, &dis) == 0 && dis.reason != 0);
	CHECK(t_getstate(cli) == T_IDLE);

	/* the endpoint of a broken connection is as good as new */
	callto(&call, &saddr);
	CHECK(t_connect(cli, &call, NULL) == 0);
	CHECK(t_getstate(cli) == T_DATAXFER);
	return 0;
}
