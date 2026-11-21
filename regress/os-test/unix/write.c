/* Test writing data without a specified destination. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	char x = 'x';
	// send is supposed to fail ENOTCONN because the socket is not connected.
	// Haiku will fail EDESTADDRREQ, but this error is wrong, because it is used
	// only for non-connection mode sockets and AF_UNIX SOCK_STREAM is
	// connection mode.
	if ( write(fd, &x, sizeof(x)) < 0 )
		err(1, "write");
	int errnum;
	socklen_t errnumlen = sizeof(errnum);
	if ( getsockopt(fd, SOL_SOCKET, SO_ERROR, &errnum, &errnumlen) < 0 )
		err(1, "getsockopt: SO_ERROR");
	if ( errnum )
	{
		errno = errnum;
		err(1, "SO_ERROR");
	}
	return 0;
}
