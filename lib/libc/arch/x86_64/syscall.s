# long __syscall6(long n, long a, long b, long c, long d, long e, long f)
# Raw system call; returns the kernel's result (-errno on failure).
	.text
	.globl	__syscall6
	.hidden	__syscall6
	.type	__syscall6,@function
__syscall6:
	mov	%rdi,%rax
	mov	%rsi,%rdi
	mov	%rdx,%rsi
	mov	%rcx,%rdx
	mov	%r8,%r10
	mov	%r9,%r8
	mov	8(%rsp),%r9
	syscall
	ret
	.size	__syscall6,.-__syscall6

# struct __pthread *__get_tp(void): the thread pointer (%fs:0 holds it).
	.globl	__get_tp
	.hidden	__get_tp
	.type	__get_tp,@function
__get_tp:
	mov	%fs:0,%rax
	ret
	.size	__get_tp,.-__get_tp

# void __restore_rt(void): signal-return trampoline for SA_RESTORER.
	.globl	__restore_rt
	.hidden	__restore_rt
	.type	__restore_rt,@function
__restore_rt:
	mov	$15,%eax		# SYS_rt_sigreturn
	syscall
	.size	__restore_rt,.-__restore_rt
	.section .note.GNU-stack,"",@progbits
