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

#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "syscall.h"

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	long r;
	if (off & 4095) { errno = EINVAL; return MAP_FAILED; }
	if (len >= PTRDIFF_MAX) { errno = ENOMEM; return MAP_FAILED; }
	r = __syscall6x(SYS_mmap, addr, len, prot, flags, fd, off);
	/* POSIX wants EINVAL for a mapping with neither MAP_SHARED nor MAP_PRIVATE */
	return (void *)__syscall_ret((unsigned long)r);
}

/* the kernel rounds an unaligned address down */
int munmap(void *a, size_t n)
{
	if ((uintptr_t)a & 4095) {
		errno = EINVAL;
		return -1;
	}
	return (int)__sys2(SYS_munmap, a, n);
}
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
	return (int)-__syscall3(SYS_madvise, a, n, advice);
}

/* shared memory objects: the kernel's shm_open (named objects) */
/* the kernel wants a leading slash (a name without one gets it) and has no NAME_MAX */
static const char *shmname(const char *name, char *buf)
{
	size_t n = strlen(name), c = 0, i;
	for (i = 0; i < n; i++) {
		if (name[i] == '/' && i > 0) {	/* one slash at most, the first */
			errno = EINVAL;
			return 0;
		}
		c = name[i] == '/' ? 0 : c + 1;
		if (c > NAME_MAX) {
			errno = ENAMETOOLONG;
			return 0;
		}
	}
	if (name[0] == '/')
		return name;
	if (n + 2 > PATH_MAX) {
		errno = ENAMETOOLONG;
		return 0;
	}
	buf[0] = '/';
	memcpy(buf + 1, name, n + 1);
	return buf;
}

int shm_open(const char *name, int flag, mode_t mode)
{
	char buf[PATH_MAX];
	if (!(name = shmname(name, buf)))
		return -1;
	return (int)__sys5(SYS_shm_open2, name, flag | O_CLOEXEC, mode, 0, 0);
}

int shm_unlink(const char *name)
{
	char buf[PATH_MAX];
	if (!(name = shmname(name, buf)))
		return -1;
	return (int)__sys1(SYS_shm_unlink, name);
}
