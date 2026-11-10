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

/* psignal(), psiginfo(): messages on standard error.
 * TODO(stdio): write through stderr once <stdio.h> exists, as POSIX
 * requires (orientation, error indicator). */
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

static void out(const char *a, const char *b)
{
	struct { const void *p; size_t n; } v[4];
	int n = 0;
	if (a && *a) { v[n].p = a; v[n++].n = strlen(a); v[n].p = ": "; v[n++].n = 2; }
	v[n].p = b; v[n++].n = strlen(b);
	v[n].p = "\n"; v[n++].n = 1;
	__syscall3(SYS_writev, 2, v, n);
}

void psignal(int sig, const char *msg)
{
	int e = errno;
	out(msg, strsignal(sig));
	errno = e;
}

void psiginfo(const siginfo_t *si, const char *msg)
{
	psignal(si->si_signo, msg);
}
