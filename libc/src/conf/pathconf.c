#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <errno.h>
#include "syscall.h"

static long value(int name, long namemax)
{
	switch (name) {
	case _PC_2_SYMLINKS: return 1;
	case _PC_ALLOC_SIZE_MIN: return 4096;
	case _PC_ASYNC_IO: return 1;
	case _PC_CHOWN_RESTRICTED: return 1;
	case _PC_FALLOC: return 1;
	case _PC_FILESIZEBITS: return 64;
	case _PC_LINK_MAX: return 65000;
	case _PC_MAX_CANON: return MAX_CANON;
	case _PC_MAX_INPUT: return MAX_INPUT;
	case _PC_NAME_MAX: return namemax > 0 ? namemax : NAME_MAX;
	case _PC_NO_TRUNC: return 1;
	case _PC_PATH_MAX: return PATH_MAX;
	case _PC_PIPE_BUF: return PIPE_BUF;
	case _PC_PRIO_IO: return 1;
	case _PC_REC_INCR_XFER_SIZE: return 4096;
	case _PC_REC_MAX_XFER_SIZE: return 4096;
	case _PC_REC_MIN_XFER_SIZE: return 4096;
	case _PC_REC_XFER_ALIGN: return 4096;
	case _PC_SYMLINK_MAX: return -1;          /* no limit */
	case _PC_SYNC_IO: return 1;
	case _PC_TEXTDOMAIN_MAX: return (namemax > 0 ? namemax : NAME_MAX) - 3;
	case _PC_TIMESTAMP_RESOLUTION: return 1;  /* nanoseconds */
	case _PC_VDISABLE: return 0;
	}
	errno = EINVAL;
	return -1;
}

long fpathconf(int fd, int name)
{
	struct statvfs v;
	if (fstatvfs(fd, &v)) return -1;
	return value(name, (long)v.f_namemax);
}

long pathconf(const char *path, int name)
{
	struct statvfs v;
	if (statvfs(path, &v)) return -1;
	return value(name, (long)v.f_namemax);
}
