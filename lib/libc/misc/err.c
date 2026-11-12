/*
 * Copyright (c) 2026, asprintf
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

/* err(3): the BSD diagnostic functions, with OpenBSD's text and behaviour:
   "progname: " (the basename of argv[0]), the formatted message, and for
   the err, warn and errc, warnc forms ": " and the text of errno or of
   the given code. */
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *__progname;

static void vwarnerr(int useerrno, int e, const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: ", __progname ? __progname : "?");
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		if (useerrno) fputs(": ", stderr);
	}
	if (useerrno) fputs(strerror(e), stderr);
	fputc('\n', stderr);
}

void vwarn(const char *fmt, va_list ap) { vwarnerr(1, errno, fmt, ap); }
void vwarnc(int code, const char *fmt, va_list ap) { vwarnerr(1, code, fmt, ap); }
void vwarnx(const char *fmt, va_list ap) { vwarnerr(0, 0, fmt, ap); }
void verr(int code, const char *fmt, va_list ap) { vwarnerr(1, errno, fmt, ap); exit(code); }
void verrc(int eval, int code, const char *fmt, va_list ap) { vwarnerr(1, code, fmt, ap); exit(eval); }
void verrx(int code, const char *fmt, va_list ap) { vwarnerr(0, 0, fmt, ap); exit(code); }

void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(1, errno, fmt, ap);
	va_end(ap);
}

void warnc(int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(1, code, fmt, ap);
	va_end(ap);
}

void warnx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(0, 0, fmt, ap);
	va_end(ap);
}

void err(int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(1, errno, fmt, ap);
	va_end(ap);
	exit(code);
}

void errc(int eval, int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(1, code, fmt, ap);
	va_end(ap);
	exit(eval);
}

void errx(int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnerr(0, 0, fmt, ap);
	va_end(ap);
	exit(code);
}
