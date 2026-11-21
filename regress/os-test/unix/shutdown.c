/* Test shutdown for read and write on a freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	// shutdown is supposed to fail ENOTCONN because the socket was not
	// connected. However, shutdown will somehow succeed on many systems
	// including Linux and the BSDs (except FreeBSD), but POSIX says the call
	// shall fail ENOTCONN in this case.
	if ( shutdown(fd, SHUT_RDWR) < 0 )
		err(1, "shutdown");
	return 0;
}
