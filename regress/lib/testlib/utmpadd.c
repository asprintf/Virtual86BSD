/* utmpadd - put a login in utmpx: utmpadd LINE USER (the line as `pts/3').
 * (for tests of programs that look at who is logged in). */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <utmpx.h>

int main(int argc, char *argv[])
{
	struct utmpx u;
	struct timeval tv;
	if (argc != 3) {
		fprintf(stderr, "usage: utmpadd LINE USER\n");
		return 2;
	}
	memset(&u, 0, sizeof u);
	u.ut_type = USER_PROCESS;
	u.ut_pid = getpid();
	snprintf(u.ut_line, sizeof u.ut_line, "%s", argv[1]);
	snprintf(u.ut_user, sizeof u.ut_user, "%s", argv[2]);
	gettimeofday(&tv, NULL);
	u.ut_tv.tv_sec = tv.tv_sec;
	u.ut_tv.tv_usec = tv.tv_usec;
	setutxent();
	pututxline(&u);
	endutxent();
	return 0;
}
