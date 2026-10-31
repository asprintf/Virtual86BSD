#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include <bits/features.h>
#define __NEED_dev_t
#define __NEED_ino_t
#define __NEED_mode_t
#define __NEED_nlink_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_off_t
#define __NEED_time_t
#define __NEED_blksize_t
#define __NEED_blkcnt_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>
#include <bits/stat_mode.h>

/* The kernel's x86-64 layout. */
struct stat {
	dev_t st_dev;
	ino_t st_ino;
	nlink_t st_nlink;
	mode_t st_mode;
	uid_t st_uid;
	gid_t st_gid;
	unsigned int __pad0;
	dev_t st_rdev;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	long __reserved[3];
};
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Message queues, semaphores and shared memory are not distinct file types. */
#define S_TYPEISMQ(buf)  0
#define S_TYPEISSEM(buf) 0
#define S_TYPEISSHM(buf) 0
#define S_TYPEISTMO(buf) 0

#define UTIME_NOW  0x3fffffff
#define UTIME_OMIT 0x3ffffffe

__BEGIN_DECLS
int    chmod(const char *, mode_t);
int    fchmod(int, mode_t);
int    fchmodat(int, const char *, mode_t, int);
int    fstat(int, struct stat *);
int    fstatat(int, const char *__RESTRICT, struct stat *__RESTRICT, int);
int    futimens(int, const struct timespec [2]);
int    lstat(const char *__RESTRICT, struct stat *__RESTRICT);
int    mkdir(const char *, mode_t);
int    mkdirat(int, const char *, mode_t);
int    mkfifo(const char *, mode_t);
int    mkfifoat(int, const char *, mode_t);
int    mknod(const char *, mode_t, dev_t);
int    mknodat(int, const char *, mode_t, dev_t);
int    stat(const char *__RESTRICT, struct stat *__RESTRICT);
mode_t umask(mode_t);
int    utimensat(int, const char *, const struct timespec [2], int);
__END_DECLS
#endif
