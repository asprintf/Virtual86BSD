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

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

#define AT_EMPTY_PATH_ 0x1000

int __openat_flags(int dirfd, const char *path, int flags, mode_t mode)
{
	int native = (flags & O_CLOFORK) && __clofork_native();
	long r = __syscall_cp4(SYS_openat, dirfd, path, native ? flags : flags & ~O_CLOFORK, mode);
	if (r >= 0 && (flags & O_PATH)) {
		/* O_EXEC and O_SEARCH are Linux's O_PATH, which opens without
		 * looking at the permissions: POSIX wants EACCES when execute
		 * (search) permission is denied.  By the effective IDs, on the
		 * file opened (kernels before 5.8: not checked). */
		long a = __syscall4(SYS_faccessat2, r, "", X_OK, AT_EMPTY_PATH_ | AT_EACCESS);
		if (a < 0 && a != -ENOSYS) {
			__syscall1(SYS_close, r);
			r = a;
		}
	}
	if (r >= 0 && (flags & O_CLOFORK) && !native && __clofork_set((int)r, 1) < 0) {
		__syscall1(SYS_close, r);
		return -1;
	}
	return (int)__syscall_ret((unsigned long)r);
}
