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
#include <stdarg.h>
#include "libc.h"

int __execvpe(const char *, char *const [], char *const []);

/* Count the arguments up to and including the terminating null pointer. */
#define COUNT(ap, first, n) do { va_start(ap, first); n = 1; \
	while (va_arg(ap, const char *)) { n++; } va_end(ap); } while (0)

int execl(const char *path, const char *arg, ...)
{
	va_list ap;
	int n, i;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		va_end(ap);
		return execve(path, argv, __environ);
	}
}

int execlp(const char *file, const char *arg, ...)
{
	va_list ap;
	int n, i;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		va_end(ap);
		return __execvpe(file, argv, __environ);
	}
}

int execle(const char *path, const char *arg, ...)
{
	va_list ap;
	int n, i;
	char **envp;
	COUNT(ap, arg, n);
	{
		char *argv[n + 1];
		va_start(ap, arg);
		argv[0] = (char *)arg;
		for (i = 1; i <= n; i++) argv[i] = va_arg(ap, char *);
		envp = va_arg(ap, char **);
		va_end(ap);
		return execve(path, argv, envp);
	}
}
