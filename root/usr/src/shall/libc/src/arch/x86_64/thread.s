# Thread primitives (see src/internal/pthread_impl.h).
	.text

# long __syscall_cp_asm(volatile int *cancel, long nr, a, b, c, d, e, f)
# A cancellation point: if *cancel is set, or becomes set before the
# system call has started (SIGCANCEL's handler then moves the program
# counter from [__cp_begin, __cp_end) to __cp_cancel), __cancel() is
# called instead.
	.globl	__syscall_cp_asm
	.hidden	__syscall_cp_asm
	.type	__syscall_cp_asm,@function
	.globl	__cp_begin
	.hidden	__cp_begin
	.globl	__cp_end
	.hidden	__cp_end
	.globl	__cp_cancel
	.hidden	__cp_cancel
__syscall_cp_asm:
__cp_begin:
	mov	(%rdi),%eax
	test	%eax,%eax
	jnz	__cp_cancel
	mov	%rsi,%rax
	mov	%rdx,%rdi
	mov	%rcx,%rsi
	mov	%r8,%rdx
	mov	%r9,%r10
	mov	8(%rsp),%r8
	mov	16(%rsp),%r9
	syscall
__cp_end:
	ret
__cp_cancel:
	jmp	__cancel
	.size	__syscall_cp_asm,.-__syscall_cp_asm

# int __clone(int (*fn)(void *), void *stack, int flags, void *arg,
#             int *ptid, void *tls, volatile int *ctid)
# The child runs fn(arg) on the new stack and exits with its value.
	.globl	__clone
	.hidden	__clone
	.type	__clone,@function
__clone:
	mov	$56,%eax		# SYS_clone(flags, stack, ptid, ctid, tls)
	mov	%rdi,%r11
	mov	%rdx,%rdi
	mov	%r8,%rdx
	mov	8(%rsp),%r10
	mov	%r9,%r8
	mov	%r11,%r9		# fn survives the system call in %r9
	and	$-16,%rsi
	sub	$8,%rsi
	mov	%rcx,(%rsi)		# arg, popped by the child
	syscall
	test	%eax,%eax
	jnz	1f
	xor	%ebp,%ebp
	pop	%rdi
	call	*%r9
	mov	%eax,%edi
	mov	$60,%eax		# SYS_exit
	syscall
	hlt
1:	ret
	.size	__clone,.-__clone

# _Noreturn void __unmapself(void *base, size_t size): unmap the stack
# the caller runs on, then exit the thread, touching no memory between.
	.globl	__unmapself
	.hidden	__unmapself
	.type	__unmapself,@function
__unmapself:
	mov	$11,%eax		# SYS_munmap
	syscall
	xor	%edi,%edi
	mov	$60,%eax		# SYS_exit
	syscall
	hlt
	.size	__unmapself,.-__unmapself

	.section .note.GNU-stack,"",@progbits
