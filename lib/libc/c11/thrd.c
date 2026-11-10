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

/* <threads.h>: threads and thread-specific storage, on the pthread
 * implementation (whose __ names keep the ISO namespace clean). */
#include <errno.h>
#include <string.h>
#include <threads.h>
#include "pthread_impl.h"
#include "libc.h"
#include "syscall.h"

void *__pthread_getspecific(unsigned);
int __pthread_setspecific(unsigned, const void *);
int __pthread_key_create(unsigned *, void (*)(void *));
int __pthread_key_delete(unsigned);

int thrd_create(thrd_t *t, thrd_start_t fn, void *arg)
{
	long attr[7];
	int r;
	memset(attr, 0, sizeof attr);
	attr[0] = 2L << 20;                    /* stack size */
	attr[1] = (long)__libc.page_size;      /* guard */
	attr[6] = 2;                           /* the start routine returns int */
	r = __pthread_create(t, attr, (void *(*)(void *))(void *)fn, arg);
	return !r ? thrd_success : r == EAGAIN ? thrd_nomem : thrd_error;
}

_Noreturn void thrd_exit(int r)
{
	__pthread_exit((void *)(long)r);
}

int thrd_join(thrd_t t, int *res)
{
	void *p;
	if (__pthread_join(t, &p))
		return thrd_error;
	if (res)
		*res = (int)(long)p;
	return thrd_success;
}

int thrd_detach(thrd_t t) { return __pthread_detach(t) ? thrd_error : thrd_success; }
thrd_t thrd_current(void) { return __pthread_self(); }
int thrd_equal(thrd_t a, thrd_t b) { return a == b; }
void thrd_yield(void) { __syscall0(SYS_sched_yield); }

/* 0, -1 when interrupted by a signal, -2 on another error */
int thrd_sleep(const struct timespec *d, struct timespec *rem)
{
	long r = __syscall_cp2(SYS_nanosleep, d, rem);
	return !r ? 0 : r == -EINTR ? -1 : -2;
}

int tss_create(tss_t *k, tss_dtor_t dtor) { return __pthread_key_create(k, dtor) ? thrd_error : thrd_success; }
void tss_delete(tss_t k) { __pthread_key_delete(k); }
void *tss_get(tss_t k) { return __pthread_getspecific(k); }
int tss_set(tss_t k, void *v) { return __pthread_setspecific(k, v) ? thrd_error : thrd_success; }

int __pthread_once(int *, void (*)(void));
void call_once(once_flag *f, void (*fn)(void)) { __pthread_once(f, fn); }
