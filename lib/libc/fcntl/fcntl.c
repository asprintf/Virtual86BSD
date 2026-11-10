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
#include <stdarg.h>
#include <errno.h>
#include "syscall.h"
#include "clofork.h"

int fcntl(int fd, int cmd, ...)
{
	unsigned long arg;
	long r;
	va_list ap;
	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);

	switch (cmd) {
	case F_SETLKW:
	case F_OFD_SETLKW:
		return (int)__sys_cp3(SYS_fcntl, fd, cmd, arg);
	case F_GETFD:
		r = __syscall2(SYS_fcntl, fd, F_GETFD);
		if (__clofork_native()) return (int)__syscall_ret((unsigned long)r);
		if (r >= 0 && __clofork_get(fd)) r |= FD_CLOFORK;
		return (int)__syscall_ret((unsigned long)r);
	case F_SETFD:
		if (__clofork_native()) return (int)__sys3(SYS_fcntl, fd, F_SETFD, arg & (FD_CLOEXEC | FD_CLOFORK));
		r = __syscall3(SYS_fcntl, fd, F_SETFD, arg & FD_CLOEXEC);
		if (r >= 0 && (__clofork_any || (arg & FD_CLOFORK)) &&
		    __clofork_set(fd, (arg & FD_CLOFORK) != 0) < 0) return -1;
		return (int)__syscall_ret((unsigned long)r);
	case F_DUPFD_CLOFORK:
		if (__clofork_native()) return (int)__sys3(SYS_fcntl, fd, F_DUPFD_CLOFORK, arg);
		r = __syscall3(SYS_fcntl, fd, F_DUPFD, arg);
		if (r >= 0 && __clofork_set((int)r, 1) < 0) {
			__syscall1(SYS_close, r);
			return -1;
		}
		return (int)__syscall_ret((unsigned long)r);
	case F_GETOWN: {
		/* plain F_GETOWN cannot report all group owners unambiguously */
		struct f_owner_ex ex;
		r = __syscall3(SYS_fcntl, fd, F_GETOWN_EX, &ex);
		if (r == -EINVAL) return (int)__syscall3(SYS_fcntl, fd, F_GETOWN, 0);
		if (r < 0) return (int)__syscall_ret((unsigned long)r);
		return ex.type == F_OWNER_PGRP ? -ex.pid : ex.pid;
	}
	case F_GETFL:
		/* the kernel adds O_LARGEFILE, which means nothing on 64-bit */
		r = __syscall2(SYS_fcntl, fd, F_GETFL);
		if (r >= 0) r &= ~0100000L;
		return (int)__syscall_ret((unsigned long)r);
	case F_SETFL:
		return (int)__sys3(SYS_fcntl, fd, cmd, arg & ~(unsigned long)O_CLOFORK);
	default:
		return (int)__sys3(SYS_fcntl, fd, cmd, arg);
	}
}
