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

/* Thread-specific data.  Each thread has an array of PTHREAD_KEYS_MAX
 * values (in its mapping; the main thread's is static, attached on its
 * first pthread_setspecific). */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include "pthread_impl.h"
#include "lock.h"

static void (*keys[PTHREAD_KEYS_MAX])(void *);
static volatile int key_lock;
static void *main_tsd[PTHREAD_KEYS_MAX];

static void nodtor(void *p) { (void)p; }

int __pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
	unsigned i;
	if (!dtor)
		dtor = nodtor;
	LOCK(&key_lock);
	for (i = 0; i < PTHREAD_KEYS_MAX; i++)
		if (!keys[i]) {
			keys[i] = dtor;
			*k = i;
			UNLOCK(&key_lock);
			return 0;
		}
	UNLOCK(&key_lock);
	return EAGAIN;
}

/* the values in every thread are cleared, so a new key starts at NULL */
int __pthread_key_delete(pthread_key_t k)
{
	struct __pthread *self = __pthread_self(), *t;
	if (k >= PTHREAD_KEYS_MAX || !keys[k])
		return EINVAL;
	__tl_lock();
	LOCK(&key_lock);
	t = self;
	do {
		if (t->tsd)
			t->tsd[k] = 0;
		t = t->next;
	} while (t != self);
	keys[k] = 0;
	UNLOCK(&key_lock);
	__tl_unlock();
	return 0;
}

void *__pthread_getspecific(pthread_key_t k)
{
	struct __pthread *self = __pthread_self();
	return self->tsd && k < PTHREAD_KEYS_MAX ? self->tsd[k] : 0;
}

int __pthread_setspecific(pthread_key_t k, const void *v)
{
	struct __pthread *self = __pthread_self();
	if (k >= PTHREAD_KEYS_MAX)
		return EINVAL;
	if (!self->tsd)
		self->tsd = main_tsd;
	self->tsd[k] = (void *)v;
	self->tsd_used = 1;
	return 0;
}

void __pthread_tsd_run_dtors(void)
{
	struct __pthread *self = __pthread_self();
	int i, j;
	for (j = 0; self->tsd && self->tsd_used && j < PTHREAD_DESTRUCTOR_ITERATIONS; j++) {
		self->tsd_used = 0;
		for (i = 0; i < PTHREAD_KEYS_MAX; i++) {
			void *v = self->tsd[i];
			void (*d)(void *) = keys[i];
			if (v && d) {
				self->tsd[i] = 0;
				if (d != nodtor)
					d(v);
			}
		}
	}
}

extern __typeof(__pthread_key_create) pthread_key_create __attribute__((weak, alias("__pthread_key_create")));
extern __typeof(__pthread_key_delete) pthread_key_delete __attribute__((weak, alias("__pthread_key_delete")));
extern __typeof(__pthread_getspecific) pthread_getspecific __attribute__((weak, alias("__pthread_getspecific")));
extern __typeof(__pthread_setspecific) pthread_setspecific __attribute__((weak, alias("__pthread_setspecific")));
