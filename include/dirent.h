#ifndef _DIRENT_H
#define _DIRENT_H
#include <bits/features.h>
#define __NEED_ino_t
#define __NEED_off_t
#define __NEED_reclen_t
#define __NEED_size_t
#define __NEED_ssize_t
#include <bits/alltypes.h>

typedef struct __dirstream DIR;

/* Both structures have the layout of Linux's struct linux_dirent64, so
 * posix_getdents() is getdents64 and readdir() returns the kernel's
 * records in place. */
struct dirent {
	ino_t d_ino;
	off_t __d_off;
	reclen_t __d_reclen;
	unsigned char __d_type;
	char d_name[256];
};

struct posix_dent {
	ino_t d_ino;
	off_t __d_off;
	reclen_t d_reclen;
	unsigned char d_type;
	char d_name[];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
/* Not distinct file types on Linux: values never returned. */
#define DT_MQ      32
#define DT_SEM     33
#define DT_SHM     34
#define DT_TMO     35

__BEGIN_DECLS
int            alphasort(const struct dirent **, const struct dirent **);
int            closedir(DIR *);
int            dirfd(DIR *);
DIR           *fdopendir(int);
DIR           *opendir(const char *);
ssize_t        posix_getdents(int, void *, size_t, int);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *__RESTRICT, struct dirent *__RESTRICT, struct dirent **__RESTRICT);
void           rewinddir(DIR *);
int            scandir(const char *, struct dirent ***, int (*)(const struct dirent *),
                       int (*)(const struct dirent **, const struct dirent **));
#if __XSI_VISIBLE
void           seekdir(DIR *, long);
long           telldir(DIR *);
#endif
__END_DECLS
#endif
