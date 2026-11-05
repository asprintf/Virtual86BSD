	.text
	.globl get_fcw, get_mxcsr
get_fcw:
	subq $8, %rsp
	fnstcw (%rsp)
	movzwl (%rsp), %eax
	addq $8, %rsp
	ret
get_mxcsr:
	subq $8, %rsp
	.byte 0x0f, 0xae, 0x1c, 0x24  # stmxcsr (%rsp)
	movl (%rsp), %eax
	addq $8, %rsp
	ret
