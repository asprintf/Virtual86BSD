/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* [TYM] typed memory objects: the kernel's (V86BSD's kernel has pools of
 * physically contiguous memory as /dev/tym/NAME, and /dev/tymctl for
 * posix_mem_offset; Linux has none, and these fail as for a system
 * without typed memory: sysconf(_SC_TYPED_MEMORY_OBJECTS) says -1). */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "syscall.h"

#define TYM_SETFLAGS 0x54d1
#define TYM_GETINFO  0x54d2
#define TYM_OFFSET   0x54d3
#define TFLAGS (POSIX_TYPED_MEM_ALLOCATE | POSIX_TYPED_MEM_ALLOCATE_CONTIG | POSIX_TYPED_MEM_MAP_ALLOCATABLE)

int posix_typed_mem_open(const char *name, int oflag, int tflag)
{
	char path[PATH_MAX];
	size_t n;
	int fd, acc = oflag & O_ACCMODE;
	if ((tflag & ~TFLAGS) || (tflag & (tflag - 1)) ||       /* at most one */
	    (acc != O_RDONLY && acc != O_WRONLY && acc != O_RDWR) ||
	    (oflag & ~(O_ACCMODE | O_CLOEXEC | O_CLOFORK))) {
		errno = EINVAL;
		return -1;
	}
	while (*name == '/')
		name++;
	n = strlen(name);
	if (n > NAME_MAX || n + sizeof "/dev/tym/" > sizeof path) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if (!n || strchr(name, '/')) {
		errno = ENOENT;
		return -1;
	}
	memcpy(path, "/dev/tym/", 9);
	memcpy(path + 9, name, n + 1);
	if ((fd = open(path, oflag)) < 0)
		return -1;
	if (__syscall3(SYS_ioctl, fd, TYM_SETFLAGS, tflag) < 0) {   /* not typed memory after all */
		close(fd);
		errno = ENOENT;
		return -1;
	}
	return fd;
}

int posix_typed_mem_get_info(int fd, struct posix_typed_mem_info *info)
{
	unsigned long len;
	long r = __syscall3(SYS_ioctl, fd, TYM_GETINFO, &len);
	if (r < 0)
		return r == -EBADF ? EBADF : ENODEV;
	memset(info, 0, sizeof *info);
	info->posix_tmi_length = len;
	return 0;
}

int posix_mem_offset(const void *__RESTRICT addr, size_t len, off_t *__RESTRICT off,
                     size_t *__RESTRICT contig_len, int *__RESTRICT fildes)
{
	struct { unsigned long addr, len; long off; unsigned long contig; int fd, pad; } q = {
		(unsigned long)addr, len, 0, 0, -1, 0 };
	long c = __syscall3(SYS_openat, AT_FDCWD, "/dev/tymctl", O_RDONLY | O_CLOEXEC), r;
	if (c < 0)
		return EACCES;     /* no typed memory here: addr cannot be any */
	r = __syscall3(SYS_ioctl, c, TYM_OFFSET, &q);
	__syscall1(SYS_close, c);
	if (r < 0)
		return (int)-r;
	*off = (off_t)q.off;
	*contig_len = q.contig;
	*fildes = q.fd;
	return 0;
}
