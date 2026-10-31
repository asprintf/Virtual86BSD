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
