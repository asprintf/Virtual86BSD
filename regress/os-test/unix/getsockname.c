/* Test the local address of a freshly made socket. */

#include "suite.h"

int main(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	struct sockaddr_un local = { .sun_path = "(uninitialized)" };
	socklen_t locallen = sizeof(local);
	if ( getsockname(fd, (struct sockaddr*) &local, &locallen) < 0 )
		err(1, "getpeername");
	// getpeername is supposed to give an empty address, either without the
	// sun_path array, or an empty string in sun_path.
	if ( locallen <= offsetof(struct sockaddr_un, sun_path) )
		printf("(pathless)\n");
	else if ( local.sun_path[0] )
		printf("%s\n", local.sun_path);
	else
		printf("(empty)\n");
	return 0;
}
