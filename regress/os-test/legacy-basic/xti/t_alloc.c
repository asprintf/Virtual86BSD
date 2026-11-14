/*[SUSv2]*/
/* Test whether t_alloc gives each structure with buffers of the sizes of the endpoint's t_info, and t_free takes them back; and the error strings. */

#include <stdlib.h>

#include "../xti_util.h"

int main(void)
{
	struct sockaddr_in addr;
	struct t_info info;
	int tcp = endpoint("/dev/tcp", 0, 0, &addr);
	int udp = endpoint("/dev/udp", 0, 0, &addr);
	struct t_bind* b;
	struct t_call* c;
	struct t_unitdata* u;
	struct t_optmgmt* o;
	struct t_uderr* e;
	struct t_info* i;

	CHECK(t_getinfo(tcp, &info) == 0);
	CHECK((b = t_alloc(tcp, T_BIND, T_ALL)) != NULL);
	CHECK(b->addr.buf && b->addr.maxlen >= (unsigned) info.addr && b->addr.len == 0 && b->qlen == 0);
	CHECK(t_free(b, T_BIND) == 0);
	CHECK((c = t_alloc(tcp, T_CALL, T_ADDR | T_OPT)) != NULL);
	CHECK(c->addr.buf && c->opt.buf && !c->udata.buf && c->udata.maxlen == 0 && c->opt.maxlen >= (unsigned) info.options);
	CHECK(t_free(c, T_CALL) == 0);
	/* T_ALL leaves out what the provider has no size for (user data of a connection) */
	CHECK((c = t_alloc(tcp, T_CALL, T_ALL)) != NULL && c->addr.buf && !c->udata.buf);
	CHECK(t_free(c, T_CALL) == 0);
	t_errno = 0;
	CHECK(t_alloc(tcp, T_CALL, T_UDATA) == NULL && t_errno == TSYSERR && errno == EINVAL);
	CHECK((o = t_alloc(tcp, T_OPTMGMT, T_OPT)) != NULL && o->opt.buf && o->opt.maxlen >= (unsigned) info.options);
	CHECK(t_free(o, T_OPTMGMT) == 0);
	CHECK((u = t_alloc(udp, T_UNITDATA, T_ALL)) != NULL && u->addr.buf && u->opt.buf && u->udata.buf && u->udata.maxlen > 0);
	CHECK(t_free(u, T_UNITDATA) == 0);
	CHECK((e = t_alloc(udp, T_UDERROR, T_ALL)) != NULL && e->addr.buf);
	CHECK(t_free(e, T_UDERROR) == 0);
	CHECK((i = t_alloc(-1, T_INFO, T_ALL)) != NULL);
	CHECK(t_free(i, T_INFO) == 0);
	CHECK(t_alloc(tcp, T_UNITDATA, T_ALL) == NULL && t_errno == TNOSTRUCTYPE);
	CHECK(t_alloc(udp, T_CALL, T_ALL) == NULL && t_errno == TNOSTRUCTYPE);
	CHECK(t_alloc(tcp, 99, T_ALL) == NULL && t_errno == TNOSTRUCTYPE);
	CHECK(t_alloc(-1, T_BIND, T_ALL) == NULL && t_errno == TBADF);
	{
		char dummy;
		FAILS(t_free(&dummy, 99), TNOSTRUCTYPE);
	}

	CHECK(strcmp(t_strerror(TBADADDR), "incorrect addr format") == 0);
	CHECK(strcmp(t_strerror(TPROTO), "XTI protocol error") == 0);
	CHECK(strcmp(t_strerror(1234), "1234: error unknown") == 0);
	CHECK(t_sysconf(_SC_T_IOV_MAX) >= 16);
	FAILS(t_sysconf(9999), TBADFLAG);
	return 0;
}
