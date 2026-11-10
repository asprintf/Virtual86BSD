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

/* pthread_join, pthread_detach, pthread_self, pthread_equal */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"

/* the kernel writes DT_EXITED (0) and wakes us when the thread is gone;
 * that wake is a shared one, so the wait is too */
static int join(struct __pthread *t, void **res, int cp)
{
	int state;
	if (t == __pthread_self())
		return EDEADLK;
	while ((state = t->detach_state) != DT_EXITED) {
		if (state == DT_DETACHED)
			return EINVAL;
		if (cp)
			__timedwait_cp(&t->detach_state, state, 0, 0, 0);
		else
			__wait(&t->detach_state, state, 0);
	}
	a_barrier();
	if (res)
		*res = t->result;
	if (t->map_base)
		__syscall2(SYS_munmap, t->map_base, t->map_size);
	return 0;
}

int __pthread_join(__pthread_t t, void **res)
{
	return join(t, res, 1);
}

int __pthread_detach(__pthread_t t)
{
	int s = a_cas(&t->detach_state, DT_JOINABLE, DT_DETACHED);
	if (s == DT_JOINABLE)
		return 0;
	if (s == DT_DETACHED)
		return EINVAL;
	return join(t, 0, 0);      /* exiting or gone: reclaim it now */
}

__pthread_t __pthread_self_(void) { return __pthread_self(); }
int __pthread_equal(__pthread_t a, __pthread_t b) { return a == b; }

extern __typeof(__pthread_join) pthread_join __attribute__((weak, alias("__pthread_join")));
extern __typeof(__pthread_detach) pthread_detach __attribute__((weak, alias("__pthread_detach")));
extern __typeof(__pthread_self_) pthread_self __attribute__((weak, alias("__pthread_self_")));
extern __typeof(__pthread_equal) pthread_equal __attribute__((weak, alias("__pthread_equal")));
