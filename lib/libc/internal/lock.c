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

/* Internal futex lock (Drepper's "mutex 2": 0 free, 1 locked, 2 contended). */
#include "lock.h"
#include "atomic.h"
#include "syscall.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE 128

void __futex_wait(volatile int *addr, int val)
{
	__syscall4(SYS_futex, addr, FUTEX_WAIT | FUTEX_PRIVATE, val, 0);
}

void __futex_wake(volatile int *addr, int cnt)
{
	if (cnt < 0) cnt = 0x7fffffff;
	__syscall3(SYS_futex, addr, FUTEX_WAKE | FUTEX_PRIVATE, cnt);
}

void __lock(volatile int *l)
{
	int spins;
	if (!a_cas(l, 0, 1)) return;
	for (spins = 0; spins < 100; spins++) {
		if (!*l && !a_cas(l, 0, 1)) return;
		a_spin();
	}
	while (a_swap(l, 2)) __futex_wait(l, 2);
}

void __unlock(volatile int *l)
{
	if (a_swap(l, 0) == 2) __futex_wake(l, 1);
}
