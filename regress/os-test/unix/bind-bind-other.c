/* Test binding to another address. */

#include "suite.h"

static char* tmpdir;
static char* socket_path;
static char* socket2_path;

static void cleanup(void)
{
	if ( socket_path )
		unlink(socket_path);
	if ( socket2_path )
		unlink(socket2_path);
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
	if ( !(socket2_path = join_paths(tmpdir, "socket2")) )
		err(1, "malloc");
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd < 0 )
		err(1, "socket");
	struct sockaddr_un local = { .sun_family = AF_UNIX };
	if ( sizeof(local.sun_path) <= strlen(socket_path) )
		errx(1, "path is too long for an unix socket: %s", socket_path);
	strcpy(local.sun_path, socket_path);
	if ( bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "first bind");
	struct sockaddr_un local2 = { .sun_family = AF_UNIX };
	if ( sizeof(local2.sun_path) <= strlen(socket2_path) )
		errx(1, "path is too long for an unix socket: %s", socket2_path);
	strcpy(local2.sun_path, socket2_path);
	// bind is supposed to fail EINVAL because the socket was already bound to
	// an address, and unix domain sockets are conventionally not a protocol
	// that supports being bound to a new address.
	if ( bind(fd, (struct sockaddr*) &local2, sizeof(local2)) < 0 )
		err(1, "second bind");
	return 0;
}
