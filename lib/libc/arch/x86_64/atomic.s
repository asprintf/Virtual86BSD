# Copyright (c) 2026, Joost Orchidee Winter
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Out-of-line atomic primitives, used when the compiler offers no
# inline assembly or atomic builtins (e.g. c17).  All are full barriers.
	.text
# int __a_cas(volatile int *p, int expected, int desired) -> old value
	.globl __a_cas
	.hidden __a_cas
	.type __a_cas,@function
__a_cas:
	mov	%esi,%eax
	lock cmpxchg %edx,(%rdi)
	ret
# void *__a_cas_p(void *volatile *p, void *expected, void *desired)
	.globl __a_cas_p
	.hidden __a_cas_p
	.type __a_cas_p,@function
__a_cas_p:
	mov	%rsi,%rax
	lock cmpxchg %rdx,(%rdi)
	ret
# int __a_swap(volatile int *p, int v) -> old value
	.globl __a_swap
	.hidden __a_swap
	.type __a_swap,@function
__a_swap:
	mov	%esi,%eax
	xchg	%eax,(%rdi)
	ret
# int __a_fetch_add(volatile int *p, int v) -> old value
	.globl __a_fetch_add
	.hidden __a_fetch_add
	.type __a_fetch_add,@function
__a_fetch_add:
	mov	%esi,%eax
	lock xadd %eax,(%rdi)
	ret
# void __a_store(volatile int *p, int v)
	.globl __a_store
	.hidden __a_store
	.type __a_store,@function
__a_store:
	xchg	%esi,(%rdi)
	ret
# void __a_barrier(void)
	.globl __a_barrier
	.hidden __a_barrier
	.type __a_barrier,@function
__a_barrier:
	mfence
	ret
# void __a_spin(void)
	.globl __a_spin
	.hidden __a_spin
	.type __a_spin,@function
__a_spin:
	pause
	ret
# void __a_crash(void)
	.globl __a_crash
	.hidden __a_crash
	.type __a_crash,@function
__a_crash:
	hlt
	.section .note.GNU-stack,"",@progbits
