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
#include "clofork.h"

int pipe2(int fd[2], int flags)
{
	long r;
	if (flags & ~(O_CLOEXEC | O_CLOFORK | O_NONBLOCK)) { errno = EINVAL; return -1; }
	if ((flags & O_CLOFORK) && __clofork_native())
		return (int)__sys2(SYS_pipe2, fd, flags);
	r = __syscall2(SYS_pipe2, fd, flags & ~O_CLOFORK);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	if ((flags & O_CLOFORK) && (__clofork_set(fd[0], 1) < 0 || __clofork_set(fd[1], 1) < 0)) {
		__clofork_set(fd[0], 0);
		__syscall1(SYS_close, fd[0]);
		__syscall1(SYS_close, fd[1]);
		return -1;
	}
	return 0;
}

int pipe(int fd[2]) { return pipe2(fd, 0); }
