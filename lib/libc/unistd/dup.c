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

#define F_DUP3FD 24         /* (the kernel's fcntl command; not in <fcntl.h>) */
#define F_DUP3FD_SHIFT 16

int dup(int fd) { return (int)__sys1(SYS_dup, fd); }

/* dup3 is fcntl(F_DUP3FD): the descriptor flags go in the command */
int dup3(int old, int new, int flags)
{
	int fl = 0;
	if (flags & ~(O_CLOEXEC | O_CLOFORK)) { errno = EINVAL; return -1; }
	if (old == new) { errno = EINVAL; return -1; }
	if (flags & O_CLOEXEC) fl |= FD_CLOEXEC;
	if (flags & O_CLOFORK) fl |= FD_CLOFORK;
	return (int)__sys3(SYS_fcntl, old, F_DUP3FD | (fl << F_DUP3FD_SHIFT), new);
}

int dup2(int old, int new)
{
	long r;
	if (old == new) {
		/* valid descriptor: return it unchanged */
		r = __syscall2(SYS_fcntl, old, F_GETFD);
		return r < 0 ? (int)__syscall_ret((unsigned long)r) : new;
	}
	return dup3(old, new, 0);
}
