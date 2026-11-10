#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "syscall.h"
#include "clofork.h"

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	long r;
	if (off & 4095) { errno = EINVAL; return MAP_FAILED; }
	if (len >= PTRDIFF_MAX) { errno = ENOMEM; return MAP_FAILED; }
	r = __syscall6x(SYS_mmap, addr, len, prot, flags, fd, off);
	/* POSIX wants EINVAL for a mapping with neither MAP_SHARED nor MAP_PRIVATE */
	return (void *)__syscall_ret((unsigned long)r);
}

int munmap(void *a, size_t n) { return (int)__sys2(SYS_munmap, a, n); }
int mprotect(void *a, size_t n, int p)
{
	uintptr_t s = (uintptr_t)a & -(uintptr_t)4096;
	return (int)__sys3(SYS_mprotect, s, n + ((uintptr_t)a - s), p);
}
int msync(void *a, size_t n, int f) { return (int)__sys_cp3(SYS_msync, a, n, f); }
int mlock(const void *a, size_t n) { return (int)__sys2(SYS_mlock, a, n); }
int munlock(const void *a, size_t n) { return (int)__sys2(SYS_munlock, a, n); }
int mlockall(int f) { return (int)__sys1(SYS_mlockall, f); }
int munlockall(void) { return (int)__sys0(SYS_munlockall); }

int posix_madvise(void *a, size_t n, int advice)
{
	/* Linux MADV_DONTNEED discards data, which POSIX does not allow: ignore it */
	if (advice == POSIX_MADV_DONTNEED) return 0;
	return (int)-__syscall3(SYS_madvise, a, n, advice);
}

/* Shared memory objects are files in /dev/shm. */
static int shm_path(const char *name, char *buf)
{
	size_t l;
	while (*name == '/') name++;
	l = strlen(name);
	if (!l || strchr(name, '/') || l > NAME_MAX || !strcmp(name, ".") || !strcmp(name, "..")) {
		errno = l > NAME_MAX ? ENAMETOOLONG : EINVAL;
		return -1;
	}
	memcpy(buf, "/dev/shm/", 9);
	memcpy(buf + 9, name, l + 1);
	return 0;
}

int __openat_flags(int, const char *, int, mode_t);

int shm_open(const char *name, int flag, mode_t mode)
{
	char buf[NAME_MAX + 10];
	if (shm_path(name, buf)) return -1;
	return __openat_flags(AT_FDCWD, buf, flag | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, mode);
}

int shm_unlink(const char *name)
{
	char buf[NAME_MAX + 10];
	if (shm_path(name, buf)) return -1;
	if (unlink(buf) == 0)
		return 0;
	if (errno == EPERM)
		errno = EACCES;     /* the sticky /dev/shm: POSIX says EACCES */
	return -1;
}
