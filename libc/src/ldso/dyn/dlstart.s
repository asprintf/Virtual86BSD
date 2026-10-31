# The dynamic linker's entry point (libc.so's ELF entry): the kernel
# leaves the program's argc, argv, envp and auxv on the stack.
	.text
	.globl	_dlstart
	.type	_dlstart,@function
_dlstart:
	xor	%ebp,%ebp
	mov	%rsp,%rdi		# the initial stack
	and	$-16,%rsp
	call	__dls1			# (hidden: no relocation needed)
	hlt
	.size	_dlstart,.-_dlstart

# _Noreturn void __dls_jump(size_t entry, size_t *sp): start the program
# with the stack the kernel made (%rdx: no termination function).
	.globl	__dls_jump
	.hidden	__dls_jump
	.type	__dls_jump,@function
__dls_jump:
	mov	%rsi,%rsp
	xor	%edx,%edx
	jmp	*%rdi
	.size	__dls_jump,.-__dls_jump
	.section .note.GNU-stack,"",@progbits
