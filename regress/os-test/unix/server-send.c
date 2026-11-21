/* Test send on a server socket. */

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
	int listener = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( listener < 0 )
		err(1, "socket");
	struct sockaddr_un local = { .sun_family = AF_UNIX };
	if ( sizeof(local.sun_path) <= strlen(socket_path) )
		errx(1, "path is too long for an unix socket: %s", socket_path);
	strcpy(local.sun_path, socket_path);
	if ( bind(listener, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "bind");
	if ( listen(listener, 1) < 0 )
		err(1, "listen");
	int client = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( client < 0 )
		err(1, "client socket");
#ifdef __sortix__
	alarm(1); // Catch systems that wrongly block until accept.
#endif
	if ( connect(client, (struct sockaddr*) &local, sizeof(local)) < 0 )
		err(1, "client connect");
#ifdef __sortix__
	alarm(0);
#endif
	int server = accept(listener, NULL, NULL);
	if ( server < 0 )
		err(1, "accept");
	ssize_t amount;
	char x = 'x';
	// send is supposed to fail ENOTCONN because the socket is not connected.
	// Haiku will fail EDESTADDRREQ, but this error is wrong, because it is used
	// only for non-connection mode sockets and AF_UNIX SOCK_STREAM is
	// connection mode.
	amount = send(server, &x, sizeof(x), 0);
	if ( amount < 0 )
		err(1, "server send");
	if ( amount != 1 )
		errx(1, "server send did not return 1");
	int errnum;
	socklen_t errnumlen = sizeof(errnum);
	if ( getsockopt(listener, SOL_SOCKET, SO_ERROR, &errnum, &errnumlen) < 0 )
		err(1, "getsockopt: SO_ERROR");
	if ( errnum )
	{
		errno = errnum;
		err(1, "SO_ERROR");
	}
	char c;
	amount = recv(client, &c, sizeof(c), 0);
	if ( amount < 0 )
		err(1, "client recv");
	if ( amount != 1 )
		errx(1, "client recv did not return 1");
	if ( c != 'x' )
		errx(1, "server did not receive x");
	// The test is supposed to exit 0.
	return 0;
}
