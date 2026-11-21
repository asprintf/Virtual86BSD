/* Test accept on a freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
#ifdef __GNU__
	alarm(1); // Hurd may block indefinitely instead of failing.
#endif
	// accept is supposed to fail EINVAL because the socket is not accepting
	// connections (because listen was never called).
	if ( accept(fd, NULL, NULL) < 0 )
		err(1, "accept");
	return 0;
}
