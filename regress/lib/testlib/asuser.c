/* asuser - run a command with the identity of a user, as root would after
 * login: asuser UID GID command [argument ...] (the groups are dropped). */
#define _DEFAULT_SOURCE
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "usage: asuser UID GID command [argument ...]\n");
		return 2;
	}
	if (setgroups(0, NULL) || setgid((gid_t)atol(argv[2])) || setuid((uid_t)atol(argv[1]))) {
		perror("asuser");
		return 2;
	}
	execv(argv[3], argv + 3);
	perror(argv[3]);
	return 127;
}
