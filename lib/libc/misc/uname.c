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

#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

static int kstr(int a, int b, char *out, size_t n)
{
	int mib[2] = { a, b };
	size_t l = n;
	long r = __syscall6x(SYS___sysctl, mib, 2, out, &l, 0, 0);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	while (l && (out[l - 1] == '\n' || out[l - 1] == 0)) l--;
	out[l] = 0;
	return 0;
}

#define CTL_KERN 1
#define KERN_OSTYPE 1
#define KERN_OSRELEASE 2
#define KERN_VERSION 4
#define KERN_HOSTNAME 10
#define KERN_HOSTID 11
#define CTL_HW 6
#define HW_MACHINE 1

int uname(struct utsname *u)
{
	return kstr(CTL_KERN, KERN_OSTYPE, u->sysname, sizeof u->sysname - 1) ||
	    kstr(CTL_KERN, KERN_HOSTNAME, u->nodename, sizeof u->nodename - 1) ||
	    kstr(CTL_KERN, KERN_OSRELEASE, u->release, sizeof u->release - 1) ||
	    kstr(CTL_KERN, KERN_VERSION, u->version, sizeof u->version - 1) ||
	    kstr(CTL_HW, HW_MACHINE, u->machine, sizeof u->machine - 1) ? -1 : 0;
}

int gethostname(char *name, size_t len)
{
	struct utsname u;
	size_t l;
	if (uname(&u)) return -1;
	l = strlen(u.nodename);
	if (l >= len) { errno = ENAMETOOLONG; return -1; }
	memcpy(name, u.nodename, l + 1);
	return 0;
}

long gethostid(void)
{
	int mib[2] = { CTL_KERN, KERN_HOSTID };
	unsigned long id = 0;
	size_t l = sizeof id;
	__syscall6x(SYS___sysctl, mib, 2, &id, &l, 0, 0);
	return (long)(int)id;
}
