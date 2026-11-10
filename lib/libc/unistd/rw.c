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
#include <sys/uio.h>
#include <errno.h>
#include "syscall.h"

ssize_t read(int fd, void *buf, size_t n) { return __sys_cp3(SYS_read, fd, buf, n); }
ssize_t write(int fd, const void *buf, size_t n) { return __sys_cp3(SYS_write, fd, buf, n); }
ssize_t pread(int fd, void *buf, size_t n, off_t off) { return __sys_cp4(SYS_pread64, fd, buf, n, off); }
/* At off even when the descriptor has O_APPEND (POSIX), which Linux does
   for pwritev2() with RWF_NOAPPEND (Linux 6.9); before that it appends */
#define RWF_NOAPPEND_ 0x20
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off)
{
	struct iovec v = { (void *)buf, n };
	long r = __syscall_cp6(SYS_pwritev2, fd, &v, 1, off, 0, RWF_NOAPPEND_);
	if (r == -EOPNOTSUPP || r == -ENOSYS)
		r = __syscall_cp4(SYS_pwrite64, fd, buf, n, off);
	return __syscall_ret((unsigned long)r);
}
ssize_t readv(int fd, const struct iovec *v, int n) { return __sys_cp3(SYS_readv, fd, v, n); }
ssize_t writev(int fd, const struct iovec *v, int n) { return __sys_cp3(SYS_writev, fd, v, n); }
off_t lseek(int fd, off_t off, int whence) { return __sys3(SYS_lseek, fd, off, whence); }
int fsync(int fd) { return (int)__sys_cp1(SYS_fsync, fd); }
int fdatasync(int fd) { return (int)__sys_cp1(SYS_fdatasync, fd); }
int ftruncate(int fd, off_t len) { return (int)__sys2(SYS_ftruncate, fd, len); }
int truncate(const char *p, off_t len) { return (int)__sys2(SYS_truncate, p, len); }
void sync(void) { __syscall0(SYS_sync); }
