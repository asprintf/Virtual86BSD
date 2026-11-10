# Process entry point of a position-independent (PIE) executable linked
# dynamically: as crt1.s, with __libc_start_main reached through the PLT.
	.text
	.globl	_start
	.type	_start,@function
_start:
	xor	%ebp,%ebp
	mov	%rsp,%rdi
	lea	main(%rip),%rsi
	and	$-16,%rsp
	call	__libc_start_main@PLT
	hlt
	.size	_start,.-_start
	.section .note.GNU-stack,"",@progbits
