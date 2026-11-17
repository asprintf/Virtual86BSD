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
	case F_SETOWN_EX: {
		/* the kernel's F_SETOWN: a process, or minus a process group */
		const struct f_owner_ex *ex = (const void *)arg;
		if (ex->type != F_OWNER_PID && ex->type != F_OWNER_PGRP) { errno = EINVAL; return -1; }
		return (int)__sys3(SYS_fcntl, fd, F_SETOWN, ex->type == F_OWNER_PGRP ? -ex->pid : ex->pid);
	}
	case F_GETOWN_EX: {
		struct f_owner_ex *ex = (void *)arg;
		r = __syscall3(SYS_fcntl, fd, F_GETOWN, 0);
		if (r < 0) return (int)__syscall_ret((unsigned long)r);
		ex->type = r < 0 ? F_OWNER_PGRP : F_OWNER_PID;
		ex->pid = (pid_t)(r < 0 ? -r : r);
		return 0;
	}
	default:
		return (int)__sys3(SYS_fcntl, fd, cmd, arg);
	}
}
