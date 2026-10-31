#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H
#include <bits/features.h>
#define __NEED_fsblkcnt_t
#define __NEED_fsfilcnt_t
#include <bits/alltypes.h>

struct statvfs {
	unsigned long f_bsize;
	unsigned long f_frsize;
	fsblkcnt_t f_blocks;
	fsblkcnt_t f_bfree;
	fsblkcnt_t f_bavail;
	fsfilcnt_t f_files;
	fsfilcnt_t f_ffree;
	fsfilcnt_t f_favail;
	unsigned long f_fsid;
	unsigned long f_flag;
	unsigned long f_namemax;
	int __reserved[6];
};

#define ST_RDONLY 1
#define ST_NOSUID 2

__BEGIN_DECLS
int fstatvfs(int, struct statvfs *);
int statvfs(const char *__RESTRICT, struct statvfs *__RESTRICT);
__END_DECLS
#endif
