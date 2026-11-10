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

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int chmod(const char *p, mode_t m) { return (int)__sys3(SYS_fchmodat, AT_FDCWD, p, m); }
int fchmod(int fd, mode_t m) { return (int)__sys2(SYS_fchmod, fd, m); }
mode_t umask(mode_t m) { return (mode_t)__syscall1(SYS_umask, m & 0777); }

int fchmodat(int dirfd, const char *p, mode_t m, int flag)
{
	long r;
	if (!flag) return (int)__sys3(SYS_fchmodat, dirfd, p, m);
	if (flag != AT_SYMLINK_NOFOLLOW) { errno = EINVAL; return -1; }
	/* Linux >= 6.6 has fchmodat2 with flags; symbolic links have no mode
	 * of their own, so for them the answer is EOPNOTSUPP either way. */
	r = __syscall4(SYS_fchmodat2, dirfd, p, m, flag);
	if (r != -ENOSYS) return (int)__syscall_ret((unsigned long)r);
	{
		struct stat st;
		if (fstatat(dirfd, p, &st, AT_SYMLINK_NOFOLLOW)) return -1;
		if (S_ISLNK(st.st_mode)) { errno = EOPNOTSUPP; return -1; }
		return (int)__sys3(SYS_fchmodat, dirfd, p, m);
	}
}
