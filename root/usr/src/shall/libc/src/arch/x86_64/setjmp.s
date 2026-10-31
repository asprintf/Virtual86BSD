# int setjmp(jmp_buf), void longjmp(jmp_buf, int)
# jmp_buf: rbx, rbp, r12, r13, r14, r15, rsp, rip (as glibc's layout).
	.text
	.globl	setjmp
	.type	setjmp,@function
setjmp:
	mov	%rbx,(%rdi)
	mov	%rbp,8(%rdi)
	mov	%r12,16(%rdi)
	mov	%r13,24(%rdi)
	mov	%r14,32(%rdi)
	mov	%r15,40(%rdi)
	lea	8(%rsp),%rdx
	mov	%rdx,48(%rdi)
	mov	(%rsp),%rdx
	mov	%rdx,56(%rdi)
	xor	%eax,%eax
	ret
	.size	setjmp,.-setjmp

	.globl	longjmp
	.type	longjmp,@function
longjmp:
	xor	%eax,%eax
	cmp	$1,%esi
	adc	%esi,%eax
	mov	(%rdi),%rbx
	mov	8(%rdi),%rbp
	mov	16(%rdi),%r12
	mov	24(%rdi),%r13
	mov	32(%rdi),%r14
	mov	40(%rdi),%r15
	mov	48(%rdi),%rsp
	jmp	*56(%rdi)
	.size	longjmp,.-longjmp
	.section .note.GNU-stack,"",@progbits
