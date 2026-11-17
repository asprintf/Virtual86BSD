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

/* The functions of <stdatomic.h> that its macros stand for. */
#include <stdatomic.h>

#undef atomic_thread_fence
#undef atomic_signal_fence

_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *f, memory_order m)
{
	return __c11_atomic_exchange((volatile _Atomic(_Bool) *)&f->__v, 1, m);
}

_Bool atomic_flag_test_and_set(volatile atomic_flag *f)
{
	return atomic_flag_test_and_set_explicit(f, memory_order_seq_cst);
}

void atomic_flag_clear_explicit(volatile atomic_flag *f, memory_order m)
{
	__c11_atomic_store((volatile _Atomic(_Bool) *)&f->__v, 0, m);
}

void atomic_flag_clear(volatile atomic_flag *f)
{
	atomic_flag_clear_explicit(f, memory_order_seq_cst);
}

void atomic_thread_fence(memory_order m) { __c11_atomic_thread_fence(m); }
void atomic_signal_fence(memory_order m) { __c11_atomic_signal_fence(m); }
