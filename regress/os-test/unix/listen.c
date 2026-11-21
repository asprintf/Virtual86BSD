/* Test listen on a freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	if ( listen(fd, 1) < 0 )
		err(1, "listen");
	// This test is supposed to exit 0.
	return 0;
}
