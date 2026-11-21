/* Test binding to the same address twice. */

#include "suite.h"

static char* tmpdir;
static char* socket_path;

static void cleanup(void)
{
	if ( socket_path )
		unlink(socket_path);
	if ( tmpdir )
		rmdir(tmpdir);
}

int main(void)
{
	if ( atexit(cleanup) )
		err(1, "atexit");
	tmpdir = create_tmpdir();
	if ( !(socket_path = join_paths(tmpdir, "socket")) )
		err(1, "malloc");
	int server = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( server < 0 )
		err(1, "first socket");
	struct sockaddr_un local = { .sun_family = AF_UNIX };
	if ( sizeof(local.sun_path) <= strlen(socket_path) )
		errx(1, "path is too long for an unix socket: %s", socket_path);
	strcpy(local.sun_path, socket_path);
	if ( bind(server, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "bind");
	if ( listen(server, 1) < 0 )
		err(1, "listen");
	// bind is supposed to fail either 1) EINVAL because the socket was already
	// bound to an address, and unix domain sockets are conventionally not a
	// protocol that supports being bound to a new address 2) or EADDRINUSE
	// because a socket (this one) already has the address bound.
	if ( bind(server, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "second bind");
	return 0;
}
