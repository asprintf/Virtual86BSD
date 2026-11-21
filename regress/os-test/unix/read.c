/* Test reading data without a specified destination. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	char x;
	// read is supposed to fail ENOTCONN because the socket is not connected.
	// Linux will fail EINVAL in this case for some reason, but POSIX specifies
	// ENOTCONN and all other systems do ENOTCONN.
	if ( read(fd, &x, sizeof(x)) < 0 )
		err(1, "read");
	return 0;
}
