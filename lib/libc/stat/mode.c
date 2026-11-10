#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int chmod(const char *p, mode_t m) { return (int)__sys3(SYS_fchmodat, AT_FDCWD, p, m); }
int fchmod(int fd, mode_t m) { return (int)__sys2(SYS_fchmod, fd, m); }
mode_t umask(mode_t m) { return (mode_t)__syscall1(SYS_umask, m & 0777); }

int fchmodat(int dirfd, const char *p, mode_t m, int flag)
{
	long r;
	if (!flag) return (int)__sys3(SYS_fchmodat, dirfd, p, m);
	if (flag != AT_SYMLINK_NOFOLLOW) { errno = EINVAL; return -1; }
	/* Linux >= 6.6 has fchmodat2 with flags; symbolic links have no mode
	 * of their own, so for them the answer is EOPNOTSUPP either way. */
	r = __syscall4(SYS_fchmodat2, dirfd, p, m, flag);
	if (r != -ENOSYS) return (int)__syscall_ret((unsigned long)r);
	{
		struct stat st;
		if (fstatat(dirfd, p, &st, AT_SYMLINK_NOFOLLOW)) return -1;
		if (S_ISLNK(st.st_mode)) { errno = EOPNOTSUPP; return -1; }
		return (int)__sys3(SYS_fchmodat, dirfd, p, m);
	}
}
