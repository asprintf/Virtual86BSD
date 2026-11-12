/* hardsrv.c - the guest's side of the network hardening tests (nethard.c is
 * the other side, on the host).  One process per connection:
 *
 *   port 7000  an echo server: what comes in goes back
 *   port 7002  control: "CONNECT port\n" connects to 10.9.0.1:port and
 *              answers "OK\n" or "ERR errno-name\n"; "QUIT\n" ends it all
 *
 * Run on V86BSD as root after `ifconfig em1 inet 10.9.0.2/24 up`.
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int lsock(int port)
{
	struct sockaddr_in a;
	int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;

	if (s < 0)
		return -1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((unsigned short)port);
	if (bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 128) < 0) {
		perror("hardsrv: listen");
		exit(1);
	}
	return s;
}

static void echo(int c)
{
	char b[4096];
	ssize_t n;

	while ((n = read(c, b, sizeof b)) > 0)
		if (write(c, b, (size_t)n) != n)
			break;
	_exit(0);
}

static const char *ename(int e)
{
	switch (e) {
	case ECONNREFUSED: return "ECONNREFUSED";
	case ETIMEDOUT: return "ETIMEDOUT";
	case ENETUNREACH: return "ENETUNREACH";
	case EHOSTUNREACH: return "EHOSTUNREACH";
	case ECONNRESET: return "ECONNRESET";
	default: return "EOTHER";
	}
}

static void control(int c, pid_t parent)
{
	char line[128];
	size_t n = 0;

	while (n < sizeof line - 1 && read(c, line + n, 1) == 1 && line[n] != '\n')
		n++;
	line[n] = 0;
	if (!strcmp(line, "QUIT")) {
		write(c, "OK\n", 3);
		kill(parent, SIGTERM);
		_exit(0);
	}
	if (!strncmp(line, "CONNECT ", 8)) {
		struct sockaddr_in a;
		char out[64];
		int s = socket(AF_INET, SOCK_STREAM, 0);

		memset(&a, 0, sizeof a);
		a.sin_family = AF_INET;
		a.sin_port = htons((unsigned short)atoi(line + 8));
		inet_pton(AF_INET, "10.9.0.1", &a.sin_addr);
		if (connect(s, (struct sockaddr *)&a, sizeof a) == 0)
			snprintf(out, sizeof out, "OK\n");
		else
			snprintf(out, sizeof out, "ERR %s\n", ename(errno));
		write(c, out, strlen(out));
		_exit(0);
	}
	write(c, "?\n", 2);
	_exit(0);
}

int main(void)
{
	int e = lsock(7000), k = lsock(7002), c;
	pid_t pid, parent = getpid();
	fd_set fds;

	signal(SIGCHLD, SIG_IGN);
	printf("hardsrv: ready\n");
	fflush(stdout);
	for (;;) {
		FD_ZERO(&fds);
		FD_SET(e, &fds);
		FD_SET(k, &fds);
		if (select((e > k ? e : k) + 1, &fds, NULL, NULL, NULL) < 0)
			continue;
		c = accept(FD_ISSET(e, &fds) ? e : k, NULL, NULL);
		if (c < 0)
			continue;
		if ((pid = fork()) == 0) {
			close(e);
			close(k);
			if (FD_ISSET(e, &fds))
				echo(c);
			control(c, parent);
		}
		close(c);
	}
}
