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

/* pthread_atfork: the handlers, which fork() runs through __atfork_hook
 * (prepare handlers in the reverse order of registration). */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "pthread_impl.h"
#include "lock.h"

struct handlers { void (*prepare)(void), (*parent)(void), (*child)(void); };

static struct handlers *list;
static size_t n, cap;
static volatile int lock;

extern void (*__atfork_hook)(int);

static void run(int who)
{
	size_t i;
	if (who < 0) {
		LOCK(&lock);
		for (i = n; i-- > 0;)
			if (list[i].prepare)
				list[i].prepare();
		return;
	}
	if (who)
		lock = 0;      /* the child: a fresh lock */
	for (i = 0; i < n; i++) {
		void (*f)(void) = who ? list[i].child : list[i].parent;
		if (f)
			f();
	}
	if (!who)
		UNLOCK(&lock);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	LOCK(&lock);
	if (n == cap) {
		size_t c = cap ? 2 * cap : 8;
		struct handlers *l = realloc(list, c * sizeof *l);
		if (!l) {
			UNLOCK(&lock);
			return ENOMEM;
		}
		list = l;
		cap = c;
	}
	list[n].prepare = prepare;
	list[n].parent = parent;
	list[n].child = child;
	n++;
	__atfork_hook = run;
	UNLOCK(&lock);
	return 0;
}
