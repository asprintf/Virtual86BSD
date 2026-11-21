/* Test getsockname on a listening socket. */

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
	if ( listen(fd, 1) < 0 )
		err(1, "listen");
	struct sockaddr_un name = { .sun_path = "(uninitialized)" };
	socklen_t namelen = sizeof(name);
	if ( getsockname(fd, (struct sockaddr*) &name, &namelen) < 0 )
		err(1, "getpeername");
	if ( namelen <= offsetof(struct sockaddr_un, sun_path) )
		printf("(pathless)\n");
	// getpeername is supposed to return the name of the socket.
	else if ( !strcmp(name.sun_path, socket_path) )
		printf("socket\n");
	else if ( name.sun_path[0] )
		printf("%s\n", name.sun_path);
	else
		printf("(empty)\n");
	return 0;
}
