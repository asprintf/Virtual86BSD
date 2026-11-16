	.text
	.globl rawsys6
# long rawsys6(long nr, long a, long b, long c, long d, long e, long f)
rawsys6:
	movq %rdi, %rax
	movq %rsi, %rdi
	movq %rdx, %rsi
	movq %rcx, %rdx
	movq %r8, %r10
	movq %r9, %r8
	movq 8(%rsp), %r9
	syscall
	ret
