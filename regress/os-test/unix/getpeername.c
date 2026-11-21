/* Test remote address on freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	struct sockaddr_un remote = { .sun_path = "(uninitialized)" };
	socklen_t remotelen = sizeof(remote);
	// getpeername is supposed to fail ENOTCONN because the socket is not
	// connected.
	if ( getpeername(fd, (struct sockaddr*) &remote, &remotelen) < 0 )
		err(1, "getpeername");
	if ( remotelen <= offsetof(struct sockaddr_un, sun_path) )
		printf("(pathless)\n");
	else if ( remote.sun_path[0] )
		printf("%s\n", remote.sun_path);
	else
		printf("(empty)\n");
	return 0;
}
