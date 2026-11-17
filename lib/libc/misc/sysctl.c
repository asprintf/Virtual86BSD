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

/* sysctl.c - sysctl(3), the system's variables by a path of integers
   (<sys/sysctl.h>), and getloadavg(3) on top of it.  Native only: the Linux
   ABI has no such call. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include "syscall.h"

#ifndef VM_LOADAVG
#define VM_LOADAVG 2
#endif

int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{
	return (int)__sys6(SYS___sysctl, name, namelen, oldp, oldlenp, newp, newlen);
}

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{
	return (int)__sys6(SYS___sysctlbyname, name, strlen(name), oldp, oldlenp, newp, newlen);
}

/* the kernel's name-to-number translation is the node { 0, 3 } */
int sysctlnametomib(const char *name, int *mibp, size_t *sizep)
{
	static const int oid[2] = { 0, 3 };
	size_t l = *sizep * sizeof *mibp;
	if (sysctl(oid, 2, mibp, &l, name, strlen(name)) < 0) return -1;
	*sizep = l / sizeof *mibp;
	return 0;
}

/* the 1, 5 and 15 minute load averages; how many of nelem (at most 3)
   were stored, or -1 */
int getloadavg(double loadavg[], int nelem)
{
	struct { unsigned ldavg[3]; long fscale; } la;
	size_t len = sizeof la;
	int mib[2] = { CTL_VM, VM_LOADAVG };
	int i;

	if (nelem < 0) { errno = EINVAL; return -1; }
	if (nelem > 3) nelem = 3;
	if (sysctl(mib, 2, &la, &len, 0, 0) == -1) return -1;
	for (i = 0; i < nelem; i++)
		loadavg[i] = (double)la.ldavg[i] / (double)la.fscale;
	return nelem;
}
