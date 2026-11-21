/* Test shutdown on a bound socket. */

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
	// shutdown is supposed to fail ENOTCONN because the socket was not
	// connected. However, shutdown will somehow succeed on many systems
	// including Linux and the BSDs (except FreeBSD), but POSIX says the call
	// shall fail ENOTCONN in this case.
	if ( shutdown(fd, SHUT_RDWR) < 0 )
		err(1, "shutdown");
	return 0;
}
