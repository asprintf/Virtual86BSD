# Process entry point.  The kernel leaves argc at (%rsp), followed by
# argv[], NULL, envp[], NULL, auxv[].
	.text
	.globl	_start
	.type	_start,@function
_start:
	xor	%ebp,%ebp		# mark the outermost frame
	mov	%rsp,%rdi		# arg 1: initial stack pointer
	lea	main(%rip),%rsi		# arg 2: main
	and	$-16,%rsp		# ABI stack alignment
	call	__libc_start_main
	hlt
	.size	_start,.-_start
	.section .note.GNU-stack,"",@progbits
