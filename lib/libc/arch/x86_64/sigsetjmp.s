# int sigsetjmp(sigjmp_buf, int savemask), void siglongjmp(sigjmp_buf, int)
# sigjmp_buf: the jmp_buf registers (64 bytes), the flag at 64, the
# signal mask at 72 (8 bytes as the kernel's).
	.text
	.globl	sigsetjmp
	.type	sigsetjmp,@function
sigsetjmp:
	mov	%esi,64(%rdi)
	test	%esi,%esi
	jz	1f
	mov	%rdi,%r9
	mov	$SYS_rt_sigprocmask,%eax	# rt_sigprocmask(SIG_BLOCK, 0, &env->mask, 8)
	xor	%edi,%edi
	xor	%esi,%esi
	lea	72(%r9),%rdx
	mov	$8,%r10d
	syscall
	mov	%r9,%rdi
1:	jmp	setjmp
	.size	sigsetjmp,.-sigsetjmp

	.globl	siglongjmp
	.type	siglongjmp,@function
siglongjmp:
	cmpl	$0,64(%rdi)
	je	1f
	mov	%rdi,%r8
	mov	%esi,%r9d
	mov	$SYS_rt_sigprocmask,%eax	# rt_sigprocmask(SIG_SETMASK, &env->mask, 0, 8)
	mov	$2,%edi
	lea	72(%r8),%rsi
	xor	%edx,%edx
	mov	$8,%r10d
	syscall
	mov	%r8,%rdi
	mov	%r9d,%esi
1:	jmp	longjmp
	.size	siglongjmp,.-siglongjmp
	.section .note.GNU-stack,"",@progbits
