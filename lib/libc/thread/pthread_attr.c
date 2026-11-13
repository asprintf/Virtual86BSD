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

/* Thread attributes (the layout: pthread_impl.h). */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include "pthread_impl.h"
#include "libc.h"

int pthread_attr_init(pthread_attr_t *a)
{
	memset(a, 0, sizeof *a);
	a->_a_stacksize = 2L << 20;
	a->_a_guardsize = (long)__libc.page_size;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }

int pthread_attr_getdetachstate(const pthread_attr_t *a, int *s) { *s = (int)a->_a_detach; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->_a_detach = s;
	return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *__RESTRICT a, size_t *__RESTRICT s) { *s = (size_t)a->_a_guardsize; return 0; }
int pthread_attr_setguardsize(pthread_attr_t *a, size_t s)
{
	if (s > SIZE_MAX / 8) return EINVAL;
	a->_a_guardsize = (long)s;
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *__RESTRICT a, size_t *__RESTRICT s) { *s = (size_t)a->_a_stacksize; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t s)
{
	if (s < PTHREAD_STACK_MIN || s > SIZE_MAX / 4) return EINVAL;
	a->_a_stackaddr = 0;
	a->_a_stacksize = (long)s;
	return 0;
}

int pthread_attr_getstack(const pthread_attr_t *__RESTRICT a, void **__RESTRICT addr, size_t *__RESTRICT s)
{
	/* (before a stack is set, unspecified: no address, the size) */
	*addr = (void *)a->_a_stackaddr;
	*s = (size_t)a->_a_stacksize;
	return 0;
}

/* SUSv2, SUSv3; stackaddr is the lowest address, as in FreeBSD and NetBSD */
int pthread_attr_getstackaddr(const pthread_attr_t *__RESTRICT a, void **__RESTRICT addr)
{
	*addr = (void *)a->_a_stackaddr;
	return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *a, void *addr)
{
	a->_a_stackaddr = (long)addr;
	return 0;
}

int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t s)
{
	if (s < PTHREAD_STACK_MIN || s > SIZE_MAX / 4) return EINVAL;
	a->_a_stackaddr = (long)addr;
	a->_a_stacksize = (long)s;
	return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *__RESTRICT a, int *__RESTRICT s) { *s = (int)(a->_a_sched & 1); return 0; }
int pthread_attr_setinheritsched(pthread_attr_t *a, int s)
{
	if ((unsigned)s > 1) return EINVAL;
	a->_a_sched = (a->_a_sched & ~1L) | s;
	return 0;
}

/* The policy is in bits 8-15 of _a_sched, the priority in the low half of
 * _a_prio.  A sporadic server's other parameters use bits the others
 * leave free (pthread_attr_t keeps its size): _a_prio bits 32-39 the low
 * priority, 40-47 the maximum of pending replenishments; the initial
 * budget and the replenishment period, in microseconds, are _a_sched from
 * bit 16 and _a_flags from bit 2. */
#define POLICY(a) ((int)(((a)->_a_sched >> 8) & 0xff))
#define US(ts) ((unsigned long)(ts).tv_sec * 1000000UL + (unsigned long)(ts).tv_nsec / 1000)

int pthread_attr_getschedpolicy(const pthread_attr_t *__RESTRICT a, int *__RESTRICT p) { *p = POLICY(a); return 0; }
int pthread_attr_setschedpolicy(pthread_attr_t *a, int p)
{
	if (p == SCHED_SPORADIC && sysconf(_SC_THREAD_SPORADIC_SERVER) < 0) return ENOTSUP;
	if (p != SCHED_OTHER && p != SCHED_FIFO && p != SCHED_RR && p != SCHED_SPORADIC) return EINVAL;
	a->_a_sched = (a->_a_sched & ~0xff00L) | (long)p << 8;
	return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *__RESTRICT a, struct sched_param *__RESTRICT p)
{
	unsigned long b = (unsigned long)a->_a_sched >> 16, r = (unsigned long)a->_a_flags >> 2;
	memset(p, 0, sizeof *p);
	p->sched_priority = (int)(unsigned)a->_a_prio;
	if (POLICY(a) == SCHED_SPORADIC) {
		p->sched_ss_low_priority = (int)((unsigned long)a->_a_prio >> 32 & 0xff);
		p->sched_ss_max_repl = (int)((unsigned long)a->_a_prio >> 40 & 0xff);
		p->sched_ss_init_budget.tv_sec = (time_t)(b / 1000000);
		p->sched_ss_init_budget.tv_nsec = (long)(b % 1000000 * 1000);
		p->sched_ss_repl_period.tv_sec = (time_t)(r / 1000000);
		p->sched_ss_repl_period.tv_nsec = (long)(r % 1000000 * 1000);
	}
	return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *__RESTRICT a, const struct sched_param *__RESTRICT p)
{
	/* the priority must suit the policy set (glibc likewise) */
	int pol = POLICY(a), lo = sched_get_priority_min(pol), hi = sched_get_priority_max(pol);
	if (lo != -1 && (p->sched_priority < lo || p->sched_priority > hi))
		return EINVAL;
	if (pol == SCHED_SPORADIC) {
		unsigned long b = US(p->sched_ss_init_budget), r = US(p->sched_ss_repl_period);
		if (p->sched_ss_low_priority < lo || p->sched_ss_low_priority > hi || p->sched_ss_max_repl < 1 ||
		    p->sched_ss_max_repl > sysconf(_SC_SS_REPL_MAX) || b == 0 || r < b || r >> 61 || b >> 47)
			return EINVAL;
		a->_a_prio = (long)((unsigned)p->sched_priority | (unsigned long)p->sched_ss_low_priority << 32 |
		                    (unsigned long)p->sched_ss_max_repl << 40);
		a->_a_sched = (long)(((unsigned long)a->_a_sched & 0xffff) | b << 16);
		a->_a_flags = (long)(((unsigned long)a->_a_flags & 3) | r << 2);
		return 0;
	}
	a->_a_prio = p->sched_priority;
	return 0;
}

int pthread_attr_getscope(const pthread_attr_t *__RESTRICT a, int *__RESTRICT s) { (void)a; *s = PTHREAD_SCOPE_SYSTEM; return 0; }
int pthread_attr_setscope(pthread_attr_t *a, int s)
{
	(void)a;
	if (s == PTHREAD_SCOPE_SYSTEM) return 0;
	return s == PTHREAD_SCOPE_PROCESS ? ENOTSUP : EINVAL;
}
