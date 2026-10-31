# Prologues of _init and _fini; the bodies are contributed by other objects.
	.section .init,"ax",@progbits
	.globl	_init
	.type	_init,@function
_init:
	push	%rax
	.section .fini,"ax",@progbits
	.globl	_fini
	.type	_fini,@function
_fini:
	push	%rax
	.section .note.GNU-stack,"",@progbits
