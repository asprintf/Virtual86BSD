/* Test send on a bound socket. */

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
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	struct sockaddr_un local = { .sun_family = AF_UNIX };
	if ( sizeof(local.sun_path) <= strlen(socket_path) )
		errx(1, "path is too long for an unix socket: %s", socket_path);
	strcpy(local.sun_path, socket_path);
	if ( bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "bind");
	char x = 'x';
	// send is supposed to fail ENOTCONN because the socket is not connected.
	// Haiku will fail EDESTADDRREQ, but this error is wrong, because it is used
	// only for non-connection mode sockets and AF_UNIX SOCK_STREAM is
	// connection mode.
	if ( send(fd, &x, sizeof(x), 0) < 0 )
		err(1, "send");
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
