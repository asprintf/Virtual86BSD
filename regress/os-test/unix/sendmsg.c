/* Test receiving data without a specified destination. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	char x = 'x';
	struct iovec iov = { .iov_base = &x, .iov_len = 1 };
	struct msghdr hdr =
	{
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	// send is supposed to fail ENOTCONN because the socket is not connected.
	// Haiku will fail EDESTADDRREQ, but this error is wrong, because it is used
	// only for non-connection mode sockets and AF_UNIX SOCK_STREAM is
	// connection mode.
	if ( sendmsg(fd, &hdr, 0) < 0 )
		err(1, "sendmsg");
	return 0;
}
