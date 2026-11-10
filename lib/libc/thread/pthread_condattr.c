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

/* Condition variable attributes: the clock in bits 0-7, process-shared
 * in bit 31. */
#include <errno.h>
#include <pthread.h>

int pthread_condattr_init(pthread_condattr_t *a) { a->__attr = 0; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }

int pthread_condattr_getclock(const pthread_condattr_t *__RESTRICT a, clockid_t *__RESTRICT c)
{
	*c = (clockid_t)(a->__attr & 0xff);
	return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t c)
{
	if (c != CLOCK_REALTIME && c != CLOCK_MONOTONIC)
		return EINVAL;
	a->__attr = (a->__attr & ~0xffu) | (unsigned)c;
	return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *__RESTRICT a, int *__RESTRICT s)
{
	*s = (int)(a->__attr >> 31);
	return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *a, int s)
{
	if ((unsigned)s > 1)
		return EINVAL;
	a->__attr = (a->__attr & 0x7fffffffu) | (unsigned)s << 31;
	return 0;
}
