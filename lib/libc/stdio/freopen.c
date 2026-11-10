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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

int __openat_flags(int, const char *, int, mode_t);

FILE *freopen(const char *__RESTRICT path, const char *__RESTRICT mode, FILE *__RESTRICT f)
{
	int fl = __fmodeflags(mode);
	FLOCK(f);
	__fflush_unlocked(f);
	if (!path) {
		/* change the mode of the same open file */
		if (f->fd < 0 || __syscall3(SYS_fcntl, f->fd, F_SETFL, fl & ~(O_CREAT | O_EXCL | O_TRUNC)) < 0) goto fail;
		if (fl & O_CLOEXEC) __syscall3(SYS_fcntl, f->fd, F_SETFD, FD_CLOEXEC);
	} else {
		int fd = __openat_flags(AT_FDCWD, path, fl, 0666);
		if (fd < 0) goto fail;
		if (f->fd >= 0 && fd != f->fd) {
			if (__syscall3(SYS_dup3, fd, f->fd, fl & O_CLOEXEC) < 0) { __syscall1(SYS_close, fd); goto fail; }
			__syscall1(SYS_close, fd);
		} else if (f->fd < 0) {
			f->fd = fd;
		}
	}
	f->flags = (f->flags & F_PERM) | (strchr(mode, '+') ? 0 : *mode == 'r' ? F_NOWR : F_NORD) |
	           (*mode == 'a' ? F_APP : 0);
	f->mode = 0;
	if (*mode == 'a' && !strchr(mode, '+')) __syscall3(SYS_lseek, f->fd, 0, SEEK_END);
	f->rpos = f->rend = f->wpos = f->wbase = f->wend = 0;
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;
	FUNLOCK(f);
	return f;
fail:
	FUNLOCK(f);
	fclose(f);
	return 0;
}
