/*
 * dgram.c - send one datagram to a Unix socket, for the tests of syslogd
 * (logger and syslog() always make well-formed messages; the tests need
 * the others too).
 *
 *   dgram SOCKET TEXT       sends TEXT as it is (no end added)
 *   dgram -n SOCKET TEXT    TEXT and a NUL
 *
 * Exit status: 0 sent, 1 failed.
 */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	struct sockaddr_un a;
	int nul = 0, fd;
	size_t n;

	if (argc > 1 && !strcmp(argv[1], "-n")) {
		nul = 1;
		argc--;
		argv++;
	}
	if (argc != 3 || strlen(argv[1]) >= sizeof a.sun_path) {
		fprintf(stderr, "usage: dgram [-n] socket text\n");
		return 1;
	}
	memset(&a, 0, sizeof a);
	a.sun_family = AF_UNIX;
	strcpy(a.sun_path, argv[1]);
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof a) < 0) {
		perror("dgram");
		return 1;
	}
	n = strlen(argv[2]) + (size_t)nul;
	if (send(fd, argv[2], n, 0) != (ssize_t)n) {
		perror("dgram");
		return 1;
	}
	return 0;
}
