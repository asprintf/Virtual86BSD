#include <sys/statvfs.h>
#include <string.h>
#include "syscall.h"

/* The kernel's struct statfs (x86-64). */
struct kstatfs {
	long f_type, f_bsize;
	unsigned long f_blocks, f_bfree, f_bavail, f_files, f_ffree;
	int f_fsid[2];
	long f_namelen, f_frsize, f_flags, f_spare[4];
};

#define KST_RDONLY 1
#define KST_NOSUID 2
#define KST_VALID  0x20

static void conv(struct statvfs *v, const struct kstatfs *k)
{
	memset(v, 0, sizeof *v);
	v->f_bsize = (unsigned long)k->f_bsize;
	v->f_frsize = (unsigned long)(k->f_frsize ? k->f_frsize : k->f_bsize);
	v->f_blocks = k->f_blocks;
	v->f_bfree = k->f_bfree;
	v->f_bavail = k->f_bavail;
	v->f_files = k->f_files;
	v->f_ffree = k->f_ffree;
	v->f_favail = k->f_ffree;
	v->f_fsid = (unsigned)k->f_fsid[0] | (unsigned long)(unsigned)k->f_fsid[1] << 32;
	v->f_flag = (k->f_flags & KST_VALID) ? (unsigned long)k->f_flags & (KST_RDONLY | KST_NOSUID) : 0;
	v->f_namemax = (unsigned long)k->f_namelen;
}

int statvfs(const char *__RESTRICT p, struct statvfs *__RESTRICT v)
{
	struct kstatfs k;
	if (__sys2(SYS_statfs, p, &k) < 0) return -1;
	conv(v, &k);
	return 0;
}

int fstatvfs(int fd, struct statvfs *v)
{
	struct kstatfs k;
	if (__sys2(SYS_fstatfs, fd, &k) < 0) return -1;
	conv(v, &k);
	return 0;
}
