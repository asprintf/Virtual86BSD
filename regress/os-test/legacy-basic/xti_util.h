/* helpers of the xti tests: an endpoint on 127.0.0.1, checks that name t_errno */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <xti.h>

#include "basic.h"

#define CHECK(c) do { if ( !(c) ) errx(1, "line %d: %s (t_errno %d)", __LINE__, #c, t_errno); } while ( 0 )
#define FAILS(c, e) do { t_errno = 0; if ( (c) != -1 || t_errno != (e) ) errx(1, "line %d: %s did not fail with %s (t_errno %d)", __LINE__, #c, #e, t_errno); } while ( 0 )

/* an endpoint of PROVIDER (O_NONBLOCK if NB) bound to 127.0.0.1 and an
   address of the system's choice, with QLEN; its address in *ADDR */
static inline int endpoint(const char* provider, int nb, unsigned qlen, struct sockaddr_in* addr)
{
	int fd = t_open(provider, O_RDWR | (nb ? O_NONBLOCK : 0), NULL);
	struct sockaddr_in want;
	struct t_bind req, ret;
	if ( fd < 0 )
		errx(1, "t_open %s (t_errno %d)", provider, t_errno);
	memset(&want, 0, sizeof(want));
	want.sin_family = AF_INET;
	want.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memset(&req, 0, sizeof(req));
	req.addr.len = sizeof(want);
	req.addr.buf = &want;
	req.qlen = qlen;
	memset(&ret, 0, sizeof(ret));
	ret.addr.maxlen = sizeof(*addr);
	ret.addr.buf = addr;
	CHECK(t_bind(fd, &req, &ret) == 0);
	CHECK(ret.addr.len == sizeof(*addr) && addr->sin_family == AF_INET && addr->sin_port != 0);
	return fd;
}

/* a call to ADDR */
static inline void callto(struct t_call* call, struct sockaddr_in* addr)
{
	memset(call, 0, sizeof(*call));
	call->addr.len = sizeof(*addr);
	call->addr.buf = addr;
}

/* wait up to two seconds for t_look(FD) to say EVENT */
#define expect_event(fd, event) wait_event(fd, event, __LINE__)
static inline void wait_event(int fd, int event, int line)
{
	int i, got = 0;
	for ( i = 0; i < 200; i++ )
	{
		got = t_look(fd);
		if ( got == event )
			return;
		usleep(10000);
	}
	errx(1, "line %d: t_look gave %d, not %d", line, got, event);
}
