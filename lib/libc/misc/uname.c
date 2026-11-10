#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

int uname(struct utsname *u) { return (int)__sys1(SYS_uname, u); }

int gethostname(char *name, size_t len)
{
	struct utsname u;
	size_t l;
	if (uname(&u)) return -1;
	l = strlen(u.nodename);
	if (l >= len) { errno = ENAMETOOLONG; return -1; }
	memcpy(name, u.nodename, l + 1);
	return 0;
}

long gethostid(void)
{
	/* /etc/hostid (4 bytes, native order) if present, else 0 */
	unsigned int id = 0;
	long fd = __syscall4(SYS_openat, -100, "/etc/hostid", 0, 0);
	if (fd >= 0) {
		if (__syscall3(SYS_read, fd, &id, sizeof id) != sizeof id) id = 0;
		__syscall1(SYS_close, fd);
	}
	return (long)(int)id;
}
