/*[SUSv2]*/
/* Test whether t_open gives endpoints of /dev/tcp and /dev/udp with their service types, and fails for others. */

#include "../xti_util.h"

int main(void)
{
	struct t_info info;
	int fd;
	memset(&info, 0, sizeof(info));
	fd = t_open("/dev/tcp", O_RDWR, &info);
	CHECK(fd >= 0);
	CHECK(info.servtype == T_COTS_ORD);
	CHECK(info.addr == sizeof(struct sockaddr_in) && info.options > 0 && info.tsdu == 0);
	CHECK(t_getstate(fd) == T_UNBND);
	memset(&info, 0, sizeof(info));
	CHECK(t_getinfo(fd, &info) == 0 && info.servtype == T_COTS_ORD);
	CHECK(t_close(fd) == 0);
	FAILS(t_getstate(fd), TBADF);
	FAILS(t_close(fd), TBADF);
	fd = t_open("/dev/udp", O_RDWR | O_NONBLOCK, &info);
	CHECK(fd >= 0);
	CHECK(info.servtype == T_CLTS && info.tsdu > 0 && info.etsdu == T_INVALID && info.connect == T_INVALID);
	CHECK(t_close(fd) == 0);
	FAILS(t_open("/dev/nothing", O_RDWR, NULL), TBADNAME);
	FAILS(t_open("/dev/tcp", O_RDONLY, NULL), TBADFLAG);
	FAILS(t_open("/dev/tcp", O_RDWR | O_APPEND, NULL), TBADFLAG);
	FAILS(t_getstate(-1), TBADF);
	return 0;
}
