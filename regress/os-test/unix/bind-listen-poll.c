/* Test poll on a listening socket. */

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
	struct pollfd pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN | POLLOUT;
	int num_events = poll(&pfd, 1, 0);
	if ( num_events < 0 )
		err(1, "poll");
	// accept is supposed to succeed.
	// POLLIN is not expected because accept would not succeed at this time.
	// POLLOUT is not expected listening sockets cannot be written to.
	// POLLERR is not expected because accept can succeed.
	// POLLHUP is not expected because the socket is listening.
	// POLLNVAL is unexpected because the file descriptor is valid.
	if ( num_events == 0 )
		errx(1, "poll returned 0");
	printf("0");
	if ( pfd.revents & POLLIN )
		printf(" | POLLIN");
	if ( pfd.revents & POLLPRI )
		printf(" | POLLPRI");
	if ( pfd.revents & POLLOUT )
		printf(" | POLLOUT");
#if defined(POLLRDHUP) && POLLRDHUP != POLLHUP
	if ( pfd.revents & POLLRDHUP )
		printf(" | POLLRDHUP");
#endif
	if ( pfd.revents & POLLERR )
		printf(" | POLLERR");
	if ( pfd.revents & POLLHUP )
		printf(" | POLLHUP");
#if POLLRDNORM != POLLIN
	if ( pfd.revents & POLLRDNORM )
		printf(" | POLLRDNORM");
#endif
#if POLLRDBAND != POLLPRI
	if ( pfd.revents & POLLRDBAND )
		printf(" | POLLRDBAND");
#endif
#if POLLWRNORM != POLLOUT
	if ( pfd.revents & POLLWRNORM )
		printf(" | POLLWRNORM");
#endif
#if POLLWRBAND != POLLOUT
	if ( pfd.revents & POLLWRBAND )
		printf(" | POLLWRBAND");
#endif
	putchar('\n');
	return 0;
}
