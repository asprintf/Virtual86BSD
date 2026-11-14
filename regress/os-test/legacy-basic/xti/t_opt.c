/*[SUSv2]*/
/* Test whether t_optmgmt reads, checks and negotiates the XTI, TCP and IP options with the status of each. */

#include "../xti_util.h"

struct opt1 {
	struct t_opthdr h;
	t_uscalar_t v;
};

static int optmgmt(int fd, int action, struct opt1* in, int n, struct opt1* out, int* nout, int* overall)
{
	struct t_optmgmt req, ret;
	int i;
	req.opt.len = 0;
	for ( i = 0; i < n; i++ )
		req.opt.len += in[i].h.len;
	req.opt.buf = in;
	req.flags = action;
	ret.opt.maxlen = 16 * sizeof(*out);
	ret.opt.buf = out;
	ret.opt.len = 0;
	ret.flags = 0;
	if ( t_optmgmt(fd, &req, &ret) < 0 )
		return -1;
	*nout = ret.opt.len / sizeof(*out);
	*overall = ret.flags;
	return 0;
}

static void init(struct opt1* o, unsigned level, unsigned name, t_uscalar_t v, int with_value)
{
	o->h.len = with_value ? sizeof(*o) : sizeof(o->h);
	o->h.level = level;
	o->h.name = name;
	o->h.status = 0;
	o->v = v;
}

int main(void)
{
	struct sockaddr_in addr;
	struct opt1 in[2], out[16];
	int fd = endpoint("/dev/tcp", 0, 0, &addr), n, overall;
	struct t_optmgmt req, ret;
	char big[1024];
	struct t_opthdr* h;

	/* the current value of TCP_NODELAY, off */
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, 0, 0);
	CHECK(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall) == 0);
	CHECK(n == 1 && out[0].h.status == T_SUCCESS && out[0].v == T_NO && overall == T_SUCCESS);
	/* negotiated on, and current again */
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, T_YES, 1);
	CHECK(optmgmt(fd, T_NEGOTIATE, in, 1, out, &n, &overall) == 0);
	CHECK(n == 1 && out[0].h.status == T_SUCCESS && out[0].v == T_YES);
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, 0, 0);
	CHECK(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall) == 0 && out[0].v == T_YES);
	CHECK(optmgmt(fd, T_DEFAULT, in, 1, out, &n, &overall) == 0 && out[0].v == T_NO);
	/* checked, not negotiated */
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, T_NO, 1);
	CHECK(optmgmt(fd, T_CHECK, in, 1, out, &n, &overall) == 0 && out[0].h.status == T_SUCCESS);
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, 0, 0);
	CHECK(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall) == 0 && out[0].v == T_YES);

	/* IP level, with two options */
	init(&in[0], T_INET_IP, T_IP_REUSEADDR, T_YES, 1);
	init(&in[1], T_INET_IP, T_IP_BROADCAST, T_YES, 1);
	CHECK(optmgmt(fd, T_NEGOTIATE, in, 2, out, &n, &overall) == 0);
	CHECK(n == 2 && out[0].h.status == T_SUCCESS && out[1].h.status == T_SUCCESS && overall == T_SUCCESS);
	init(&in[0], T_INET_IP, T_IP_REUSEADDR, 0, 0);
	CHECK(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall) == 0 && out[0].v == T_YES);

	/* read-only, unsupported, and the worst of them */
	init(&in[0], T_INET_TCP, T_TCP_MAXSEG, 1000, 1);
	CHECK(optmgmt(fd, T_NEGOTIATE, in, 1, out, &n, &overall) == 0);
	CHECK(out[0].h.status == T_READONLY && overall == T_READONLY);
	init(&in[0], T_INET_TCP, 0x7777, 1, 1);
	CHECK(optmgmt(fd, T_NEGOTIATE, in, 1, out, &n, &overall) == 0);
	CHECK(out[0].h.status == T_NOTSUPPORT && overall == T_NOTSUPPORT);

	/* errors: two levels, an unknown level, a short option */
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, T_NO, 1);
	init(&in[1], T_INET_IP, T_IP_REUSEADDR, T_NO, 1);
	FAILS(optmgmt(fd, T_NEGOTIATE, in, 2, out, &n, &overall), TBADOPT);
	init(&in[0], 0x4242, 1, 0, 0);
	FAILS(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall), TBADOPT);
	init(&in[0], T_INET_TCP, T_TCP_NODELAY, 0, 0);
	in[0].h.len = 3;
	FAILS(optmgmt(fd, T_CURRENT, in, 1, out, &n, &overall), TBADOPT);
	FAILS(optmgmt(fd, 0x12345, in, 1, out, &n, &overall), TBADFLAG);

	/* all the options of a level, found with the macros */
	init(&in[0], XTI_GENERIC, T_ALLOPT, 0, 0);
	req.opt.len = sizeof(in[0].h);
	req.opt.buf = in;
	req.flags = T_CURRENT;
	ret.opt.maxlen = sizeof(big);
	ret.opt.buf = big;
	CHECK(t_optmgmt(fd, &req, &ret) == 0);
	n = 0;
	for ( h = T_OPT_FIRSTHDR(&ret.opt); h; h = T_OPT_NXTHDR(&ret.opt, h) )
	{
		CHECK(h->level == XTI_GENERIC && h->status == T_SUCCESS);
		n++;
	}
	CHECK(n >= 5);
	ret.opt.maxlen = 8;
	FAILS(t_optmgmt(fd, &req, &ret), TBUFOVFLW);

	/* the linger option */
	{
		struct { struct t_opthdr h; struct t_linger l; } lin, lout;
		lin.h.len = sizeof(lin); lin.h.level = XTI_GENERIC; lin.h.name = XTI_LINGER; lin.h.status = 0;
		lin.l.l_onoff = T_YES; lin.l.l_linger = 5;
		req.opt.len = sizeof(lin); req.opt.buf = &lin; req.flags = T_NEGOTIATE;
		ret.opt.maxlen = sizeof(lout); ret.opt.buf = &lout;
		CHECK(t_optmgmt(fd, &req, &ret) == 0 && lout.h.status == T_SUCCESS);
		req.flags = T_CURRENT;
		CHECK(t_optmgmt(fd, &req, &ret) == 0 && lout.l.l_onoff == T_YES && lout.l.l_linger == 5);
	}
	return 0;
}
