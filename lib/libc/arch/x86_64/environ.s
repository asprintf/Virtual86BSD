# char **__environ / environ: one object with two names.  The library
# uses __environ internally so that ISO C functions (getenv) never pull
# in the POSIX name; environ is the application-visible alias.
	.bss
	.align	8
	.globl	__environ
	.type	__environ,@object
	.size	__environ,8
	.weak	environ
	.type	environ,@object
	.size	environ,8
__environ:
environ:
	.zero	8
	.section .note.GNU-stack,"",@progbits
