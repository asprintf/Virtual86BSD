/* Test sendto without binding. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	char x = 'x';
	// send is supposed to fail ENOTCONN because the socket is not connected.
	if ( sendto(fd, &x, sizeof(x), 0, NULL, 0) < 0 )
		err(1, "sendto");
	return 0;
}
