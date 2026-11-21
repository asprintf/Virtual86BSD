/* Test receiving data without a specified destination. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	char x;
	struct iovec iov = { .iov_base = &x, .iov_len = 1 };
	struct msghdr hdr =
	{
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	// recvmsg is supposed to fail ENOTCONN because the socket is not connected.
	// Linux will fail EINVAL in this case for some reason, but POSIX specifies
	// ENOTCONN and all other systems do ENOTCONN.
	if ( recvmsg(fd, &hdr, 0) < 0 )
		err(1, "recvmsg");
	return 0;
}
