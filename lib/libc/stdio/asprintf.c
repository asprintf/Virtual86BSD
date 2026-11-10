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

/* POSIX printf variants. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

int vasprintf(char **__RESTRICT s, const char *__RESTRICT fmt, va_list ap)
{
	va_list ap2;
	int l;
	va_copy(ap2, ap);
	l = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (l < 0 || !(*s = malloc((size_t)l + 1))) { *s = 0; return -1; }  /* *s indeterminate per POSIX; null is safest */
	return vsnprintf(*s, (size_t)l + 1, fmt, ap);
}

int asprintf(char **__RESTRICT s, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vasprintf(s, fmt, ap); va_end(ap);
	return r;
}

static size_t fd_write(FILE *f, const unsigned char *s, size_t l)
{
	return __stdio_write(f, s, l);
}

int vdprintf(int fd, const char *__RESTRICT fmt, va_list ap)
{
	unsigned char buf[BUFSIZ + UNGET];
	FILE f;
	int r;
	memset(&f, 0, sizeof f);
	f.fd = fd;
	f.lbf = EOF;
	f.write = fd_write;
	f.buf = buf + UNGET;
	f.buf_size = BUFSIZ;
	f.lockcount = 1;
	r = __vfprintf(&f, fmt, ap);
	if (f.wpos != f.wbase) f.write(&f, 0, 0);
	return (f.flags & F_ERR) ? -1 : r;
}

int dprintf(int fd, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vdprintf(fd, fmt, ap); va_end(ap);
	return r;
}
