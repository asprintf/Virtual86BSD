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

/* pthread_once: 0 not run, 1 running, 2 done, 3 running with waiters.
 * If the routine is cancelled, the state goes back to 0. */
#include <pthread.h>
#include "pthread_impl.h"
#include "atomic.h"

static void undo(void *c)
{
	if (a_swap(c, 0) == 3)
		__wake(c, -1, 1);
}

static int slow(volatile int *c, void (*f)(void))
{
	struct __ptcb cb;
	for (;;) switch (a_cas(c, 0, 1)) {
	case 0:
		__pthread_cleanup_push(&cb, undo, (void *)c);
		f();
		__pthread_cleanup_pop(&cb, 0);
		if (a_swap(c, 2) == 3)
			__wake(c, -1, 1);
		return 0;
	case 1:
		a_cas(c, 1, 3);
		/* fall through */
	case 3:
		__wait(c, 3, 1);
		continue;
	default:
		return 0;
	}
}

int __pthread_once(pthread_once_t *c, void (*f)(void))
{
	if (*(volatile int *)c == 2) {
		a_barrier();
		return 0;
	}
	return slow((volatile int *)c, f);
}

extern __typeof(__pthread_once) pthread_once __attribute__((weak, alias("__pthread_once")));
