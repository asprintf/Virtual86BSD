# rt_float128.s - c17: run-time support: the binary128 functions of the
# x86-64 ABI, whose operands and results are in %xmm registers; each
# stores them on the stack and calls its C counterpart in rt_float128.c
# with pointers to them.

	.text

	.globl __addtf3
	.type __addtf3, @function
__addtf3:
	subq $56, %rsp
	movdqu %xmm0, (%rsp)
	movdqu %xmm1, 16(%rsp)
	leaq 32(%rsp), %rdi
	leaq (%rsp), %rsi
	leaq 16(%rsp), %rdx
	call __c17_addtf3
	movdqu 32(%rsp), %xmm0
	addq $56, %rsp
	ret
	.size __addtf3, .-__addtf3

	.globl __subtf3
	.type __subtf3, @function
__subtf3:
	subq $56, %rsp
	movdqu %xmm0, (%rsp)
	movdqu %xmm1, 16(%rsp)
	leaq 32(%rsp), %rdi
	leaq (%rsp), %rsi
	leaq 16(%rsp), %rdx
	call __c17_subtf3
	movdqu 32(%rsp), %xmm0
	addq $56, %rsp
	ret
	.size __subtf3, .-__subtf3

	.globl __multf3
	.type __multf3, @function
__multf3:
	subq $56, %rsp
	movdqu %xmm0, (%rsp)
	movdqu %xmm1, 16(%rsp)
	leaq 32(%rsp), %rdi
	leaq (%rsp), %rsi
	leaq 16(%rsp), %rdx
	call __c17_multf3
	movdqu 32(%rsp), %xmm0
	addq $56, %rsp
	ret
	.size __multf3, .-__multf3

	.globl __divtf3
	.type __divtf3, @function
__divtf3:
	subq $56, %rsp
	movdqu %xmm0, (%rsp)
	movdqu %xmm1, 16(%rsp)
	leaq 32(%rsp), %rdi
	leaq (%rsp), %rsi
	leaq 16(%rsp), %rdx
	call __c17_divtf3
	movdqu 32(%rsp), %xmm0
	addq $56, %rsp
	ret
	.size __divtf3, .-__divtf3

# the comparisons: __c17_cmptf2 gives -1, 0, 1, or 2 when unordered;
# eq and ne: 0 when equal; lt and le: negative, zero, or 1 when unordered;
# gt and ge: positive, zero, or -1 when unordered; unord: nonzero
	.type cmp, @function
cmp:
	subq $40, %rsp
	movdqu %xmm0, (%rsp)
	movdqu %xmm1, 16(%rsp)
	leaq (%rsp), %rdi
	leaq 16(%rsp), %rsi
	call __c17_cmptf2
	addq $40, %rsp
	ret
	.size cmp, .-cmp

	.globl __eqtf2, __netf2, __lttf2, __letf2
	.type __eqtf2, @function
	.type __netf2, @function
	.type __lttf2, @function
	.type __letf2, @function
__eqtf2:
__netf2:
__lttf2:
__letf2:
	subq $8, %rsp
	call cmp
	addq $8, %rsp
	cmpl $2, %eax
	jne 1f
	movl $1, %eax
1:	ret

	.globl __gttf2, __getf2
	.type __gttf2, @function
	.type __getf2, @function
__gttf2:
__getf2:
	subq $8, %rsp
	call cmp
	addq $8, %rsp
	cmpl $2, %eax
	jne 1f
	movl $-1, %eax
1:	ret

	.globl __unordtf2
	.type __unordtf2, @function
__unordtf2:
	subq $8, %rsp
	call cmp
	addq $8, %rsp
	cmpl $2, %eax
	sete %al
	movzbl %al, %eax
	ret

	.section .note.GNU-stack,"",@progbits
