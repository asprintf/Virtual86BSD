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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int link(const char *a, const char *b) { return (int)__sys5(SYS_linkat, AT_FDCWD, a, AT_FDCWD, b, 0); }
int linkat(int fa, const char *a, int fb, const char *b, int flag) { return (int)__sys5(SYS_linkat, fa, a, fb, b, flag); }
int symlink(const char *a, const char *b) { return (int)__sys3(SYS_symlinkat, a, AT_FDCWD, b); }
int symlinkat(const char *a, int fd, const char *b) { return (int)__sys3(SYS_symlinkat, a, fd, b); }
int unlink(const char *p) { return (int)__sys3(SYS_unlinkat, AT_FDCWD, p, 0); }
int unlinkat(int fd, const char *p, int flag) { return (int)__sys3(SYS_unlinkat, fd, p, flag); }
int rmdir(const char *p) { return (int)__sys3(SYS_unlinkat, AT_FDCWD, p, AT_REMOVEDIR); }
int chdir(const char *p) { return (int)__sys1(SYS_chdir, p); }
int fchdir(int fd) { return (int)__sys1(SYS_fchdir, fd); }
int chown(const char *p, uid_t u, gid_t g) { return (int)__sys5(SYS_fchownat, AT_FDCWD, p, u, g, 0); }
int lchown(const char *p, uid_t u, gid_t g) { return (int)__sys5(SYS_fchownat, AT_FDCWD, p, u, g, AT_SYMLINK_NOFOLLOW); }
int fchown(int fd, uid_t u, gid_t g) { return (int)__sys3(SYS_fchown, fd, u, g); }
int fchownat(int fd, const char *p, uid_t u, gid_t g, int flag) { return (int)__sys5(SYS_fchownat, fd, p, u, g, flag); }

ssize_t readlinkat(int fd, const char *__RESTRICT p, char *__RESTRICT buf, size_t n)
{
	char dummy[1];
	if (!n) { buf = dummy; n = 1; }  /* the kernel refuses a size of 0 */
	return __sys4(SYS_readlinkat, fd, p, buf, n);
}
ssize_t readlink(const char *__RESTRICT p, char *__RESTRICT buf, size_t n) { return readlinkat(AT_FDCWD, p, buf, n); }

int access(const char *p, int mode) { return (int)__sys3(SYS_faccessat, AT_FDCWD, p, mode); }

int faccessat(int fd, const char *p, int mode, int flag)
{
	long r;
	if (!flag) return (int)__sys3(SYS_faccessat, fd, p, mode);
	if (flag & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW)) { errno = EINVAL; return -1; }
	r = __syscall4(SYS_faccessat2, fd, p, mode, flag);
	if (r != -ENOSYS) return (int)__syscall_ret((unsigned long)r);
	/* Old kernels: with equal real and effective IDs the flag changes nothing. */
	if ((flag & AT_EACCESS) && (__syscall0(SYS_getuid) != __syscall0(SYS_geteuid) ||
	    __syscall0(SYS_getgid) != __syscall0(SYS_getegid))) { errno = ENOSYS; return -1; }
	if (flag & AT_SYMLINK_NOFOLLOW) { errno = ENOSYS; return -1; }
	return (int)__sys3(SYS_faccessat, fd, p, mode);
}

