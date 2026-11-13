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

/* brk, sbrk, chroot, cuserid, getdtablesize, getpagesize (SUSv2); getwd, ualarm (SUSv2, SUSv3) */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include "syscall.h"

int brk(void *addr)
{
	if (__syscall1(SYS_brk, (long)addr) != (long)addr) {
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

void *sbrk(intptr_t incr)
{
	long old = __syscall1(SYS_brk, 0);
	if (incr == 0)
		return (void *)old;
	if ((incr > 0 && old + incr < old) || (incr < 0 && old + incr > old) ||
	    __syscall1(SYS_brk, old + incr) != old + incr) {
		errno = ENOMEM;
		return (void *)-1;
	}
	return (void *)old;
}

int chroot(const char *path)
{
	return (int)__sys1(SYS_chroot, path);
}

/* the effective user, as glibc */
char *cuserid(char *s)
{
	static char buf[L_cuserid];
	struct passwd *pw = getpwuid(geteuid());
	char *d = s ? s : buf;
	if (!pw) {
		if (s)
			*s = 0;
		return s;
	}
	strncpy(d, pw->pw_name, L_cuserid - 1);
	d[L_cuserid - 1] = 0;
	return d;
}

int getdtablesize(void)
{
	struct rlimit r;
	if (getrlimit(RLIMIT_NOFILE, &r) < 0 || r.rlim_cur > INT_MAX)
		return INT_MAX;
	return (int)r.rlim_cur;
}

int getpagesize(void)
{
	return (int)sysconf(_SC_PAGESIZE);
}

char *getwd(char *path_name)
{
	return getcwd(path_name, PATH_MAX + 1);
}

useconds_t ualarm(useconds_t useconds, useconds_t interval)
{
	struct itimerval it, old;
	it.it_value.tv_sec = useconds / 1000000;
	it.it_value.tv_usec = useconds % 1000000;
	it.it_interval.tv_sec = interval / 1000000;
	it.it_interval.tv_usec = interval % 1000000;
	if (setitimer(ITIMER_REAL, &it, &old) < 0)
		return 0;
	return (useconds_t)(old.it_value.tv_sec * 1000000 + old.it_value.tv_usec);
}
