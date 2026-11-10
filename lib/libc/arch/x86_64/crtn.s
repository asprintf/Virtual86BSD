# Epilogues of _init and _fini.
	.section .init,"ax",@progbits
	pop	%rax
	ret
	.section .fini,"ax",@progbits
	pop	%rax
	ret
	.section .note.GNU-stack,"",@progbits
