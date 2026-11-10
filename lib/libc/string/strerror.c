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

#include <string.h>
#include <errno.h>

static const char *const msgs[] = {
	[0] = "Success",
#define E(n, sym, msg) [n] = msg,
#include "../errno/errlist.h"
#undef E
};
#define NMSGS (sizeof msgs / sizeof *msgs)

/* Returns 0 for an unknown error number. */
const char *__strerror_msg(int e)
{
	if (e < 0 || (unsigned)e >= NMSGS || !msgs[e]) return 0;
	return msgs[e];
}

char *__strerror_unknown(int e, char *buf)
{
	/* "Unknown error -2147483648" fits in 32 bytes. */
	char tmp[16], *p = tmp + sizeof tmp;
	unsigned u = e < 0 ? 0U - (unsigned)e : (unsigned)e;
	*--p = 0;
	do *--p = (char)('0' + u % 10); while (u /= 10);
	if (e < 0) *--p = '-';
	memcpy(buf, "Unknown error ", 14);
	strcpy(buf + 14, p);
	return buf;
}

char *strerror(int e)
{
	static char buf[32]; /* only for unknown numbers */
	const char *m = __strerror_msg(e);
	return m ? (char *)m : __strerror_unknown(e, buf);
}

