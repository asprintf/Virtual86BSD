/* Test recvfrom without binding. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	struct sockaddr_un un;
	socklen_t unlen = sizeof(un);
	char x;
	// recvfrom is supposed to fail ENOTCONN because the socket is not
	// connected. Linux will fail EINVAL in this case for some reason, but POSIX
	// specifies ENOTCONN and all other systems do ENOTCONN.
	if ( recvfrom(fd, &x, sizeof(x), 0, (struct sockaddr*) &un, &unlen) < 0 )
		err(1, "recvfrom");
	return 0;
}
