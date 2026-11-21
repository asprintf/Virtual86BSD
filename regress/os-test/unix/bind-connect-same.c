/* Test connecting on a bound socket to the same address. */

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
		err(1, "server socket");
	struct sockaddr_un local = { .sun_family = AF_UNIX };
	if ( sizeof(local.sun_path) <= strlen(socket_path) )
		errx(1, "path is too long for an unix socket: %s", socket_path);
	strcpy(local.sun_path, socket_path);
	if ( bind(server, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "server bind");
	if ( listen(server, 1) < 0 )
		err(1, "server listen");
	int client = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( client < 0 )
		err(1, "client socket");
	// The client bind is supposed to fail EADDRINUSE because the server already
	// did bind to the same address.
	if ( bind(client, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "client bind");
#ifdef __sortix__
	alarm(1); // Catch systems that wrongly block until accept.
#endif
	if ( connect(client, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "client connect");
#ifdef __sortix__
	alarm(0);
#endif
	return 0;
}
