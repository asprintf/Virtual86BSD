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

/* Signal sets.  The library reserves signals 32-34 for itself; they can
 * be neither added to a set nor are they part of a filled set. */
#include <signal.h>
#include <string.h>
#include <errno.h>

#define RESERVED(s) ((unsigned)(s) - 32 < 3)
#define BAD(s) ((unsigned)(s) - 1 >= _NSIG - 1 || RESERVED(s))

int sigemptyset(sigset_t *set) { memset(set, 0, sizeof *set); return 0; }

int sigfillset(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	set->__bits[0] = ~(7UL << 31); /* 1-31 and 35-64; not 32-34 */
	return 0;
}

int sigaddset(sigset_t *set, int s)
{
	if (BAD(s)) { errno = EINVAL; return -1; }
	set->__bits[0] |= 1UL << (s - 1);
	return 0;
}

int sigdelset(sigset_t *set, int s)
{
	if (BAD(s)) { errno = EINVAL; return -1; }
	set->__bits[0] &= ~(1UL << (s - 1));
	return 0;
}

int sigismember(const sigset_t *set, int s)
{
	if ((unsigned)s - 1 >= _NSIG - 1) { errno = EINVAL; return -1; }
	return (int)(set->__bits[0] >> (s - 1) & 1);
}
