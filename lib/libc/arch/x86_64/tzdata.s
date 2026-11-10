# The time-zone variables, each with a library-internal name used by the
# ISO C functions and a weak POSIX/XSI name (tzname, timezone, daylight),
# which a strictly conforming ISO C program may define for itself.
	.section .rodata.str1.1,"aMS",@progbits,1
.Lutc:	.asciz	"UTC"

	.data
	.align	8
	.globl	__tzname
	.type	__tzname,@object
	.size	__tzname,16
	.weak	tzname
	.type	tzname,@object
	.size	tzname,16
__tzname:
tzname:
	.quad	.Lutc
	.quad	.Lutc

	.bss
	.align	8
	.globl	__timezone
	.type	__timezone,@object
	.size	__timezone,8
	.weak	timezone
	.type	timezone,@object
	.size	timezone,8
__timezone:
timezone:
	.zero	8

	.align	4
	.globl	__daylight
	.type	__daylight,@object
	.size	__daylight,4
	.weak	daylight
	.type	daylight,@object
	.size	daylight,4
__daylight:
daylight:
	.zero	4
	.section .note.GNU-stack,"",@progbits
