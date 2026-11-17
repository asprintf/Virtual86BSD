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

/* The interface list of the kernel (network/ifmsg.h); no POSIX names here. */
#include <errno.h>
#include <net/if.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "syscall.h"
#include "ifmsg.h"

#define NET_RT_IFLISTL_ 5

int __ifmsg_list(char **out, size_t *len)
{
	int mib[6] = { 4 /* CTL_NET */, 17 /* PF_ROUTE */, 0, 0, NET_RT_IFLISTL_, 0 };
	char *buf = 0;
	size_t n = 0;
	int tries = 0;
	for (;;) {
		long r;
		char *b;
		n = 0;
		if (__syscall6x(SYS___sysctl, mib, 6, 0, &n, 0, 0) < 0)
			break;
		n += n / 8 + 4096;       /* the table can grow meanwhile */
		if (!(b = realloc(buf, n))) { free(buf); errno = ENOMEM; return -1; }
		buf = b;
		r = __syscall6x(SYS___sysctl, mib, 6, buf, &n, 0, 0);
		if (r >= 0) {
			*out = buf;
			*len = n;
			return 0;
		}
		if (r != -ENOMEM || ++tries > 8) {
			errno = (int)-r;
			free(buf);
			return -1;
		}
	}
	free(buf);
	return -1;
}

#define SA_SIZE(sa) ((sa)->sa_len ? ((sa)->sa_len + 7UL) & ~7UL : 8UL)

const struct sockaddr *__ifmsg_addr(const struct ifmsg *m, int bit)
{
	const char *p = (const char *)m + m->len;
	int i;
	for (i = 0; i < IFM_RTAX_MAX; i++) {
		const struct sockaddr *sa = (const void *)p;
		if (!(m->addrs & (1 << i)))
			continue;
		if (1 << i == bit)
			return sa;
		p += SA_SIZE(sa);
	}
	return 0;
}
