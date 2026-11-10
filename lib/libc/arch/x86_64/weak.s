# Calls through weak references: the callee is linked in only if
# something else in the program needs it.
	.text
# void __stdio_exit_weak(void): flush stdio at exit, if stdio is used.
	.globl	__stdio_exit_weak
	.hidden	__stdio_exit_weak
	.type	__stdio_exit_weak,@function
__stdio_exit_weak:
	mov	__stdio_exit@GOTPCREL(%rip),%rax	# (through the GOT: also in a PIE)
	test	%rax,%rax
	jz	1f
	jmp	*%rax
1:	ret
	.size	__stdio_exit_weak,.-__stdio_exit_weak
	.weak	__stdio_exit
	.section .note.GNU-stack,"",@progbits
