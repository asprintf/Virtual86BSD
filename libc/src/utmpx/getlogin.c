#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include "utx_impl.h"

/* The login name of the session: the USER_PROCESS entry in the user
 * accounting database for the controlling terminal (as found on standard
 * input, output or error); failing that, the kernel's audit login uid. */
int getlogin_r(char *name, size_t size)
{
	char tty[64], *line, buf[1024];
	unsigned char r[UTX_RECSIZE];
	struct utmpx u;
	struct passwd pw, *res;
	int i, fd, e = errno, found = 0;
	unsigned long uid = 0;
	ssize_t n;
	size_t l;

	for (i = 0; i < 3; i++)
		if (!ttyname_r(i, tty, sizeof tty)) break;
	if (i < 3 && (fd = __utx_open(O_RDONLY)) >= 0) {
		line = strncmp(tty, "/dev/", 5) ? tty : tty + 5;
		while (read(fd, r, sizeof r) == (ssize_t)sizeof r) {
			__utx_decode(&u, r);
			if (u.ut_type == USER_PROCESS && !strncmp(u.ut_line, line, sizeof u.ut_line) && u.ut_user[0]) {
				found = 1;
				break;
			}
		}
		close(fd);
		if (found) {
			l = strnlen(u.ut_user, sizeof u.ut_user);
			if (l >= size) return ERANGE;
			memcpy(name, u.ut_user, l);
			name[l] = 0;
			errno = e;
			return 0;
		}
	}
	if ((fd = open("/proc/self/loginuid", O_RDONLY | O_CLOEXEC)) >= 0) {
		n = read(fd, buf, sizeof buf - 1);
		close(fd);
		if (n > 0) {
			buf[n] = 0;
			for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) uid = uid * 10 + (unsigned)(buf[i] - '0');
			if (i && uid != 0xffffffffUL && !getpwuid_r((uid_t)uid, &pw, buf, sizeof buf, &res) && res) {
				l = strlen(pw.pw_name);
				if (l >= size) return ERANGE;
				memcpy(name, pw.pw_name, l + 1);
				errno = e;
				return 0;
			}
		}
	}
	errno = e;
	return ENXIO;
}

char *getlogin(void)
{
	static char name[LOGIN_NAME_MAX];
	int r = getlogin_r(name, sizeof name);
	if (r) { errno = r; return 0; }
	return name;
}
