# <fenv.h> on x86-64: the x87 status and control words and MXCSR.
# Exception flags are the same bits in both (FE_INVALID 0x01 ...
# FE_INEXACT 0x20); the rounding mode is bits 10-11 of the x87 control
# word and bits 13-14 of MXCSR.  stmxcsr, ldmxcsr (always on (%rsp))
# and fnclex are spelled out as bytes for assemblers that lack them.
	.text

# int feclearexcept(int e)
	.globl	feclearexcept
	.type	feclearexcept,@function
feclearexcept:
	and	$0x3f,%edi
	not	%edi
	sub	$40,%rsp
	fnstenv	(%rsp)
	and	%di,4(%rsp)
	fldenv	(%rsp)
	.byte	0x0f,0xae,0x1c,0x24	# stmxcsr (%rsp)
	and	%edi,(%rsp)
	.byte	0x0f,0xae,0x14,0x24	# ldmxcsr (%rsp)
	add	$40,%rsp
	xor	%eax,%eax
	ret
	.size	feclearexcept,.-feclearexcept

# int feraiseexcept(int e): sets the flags in MXCSR (fetestexcept looks
# at both units)
	.globl	feraiseexcept
	.type	feraiseexcept,@function
feraiseexcept:
	and	$0x3f,%edi
	sub	$8,%rsp
	.byte	0x0f,0xae,0x1c,0x24	# stmxcsr (%rsp)
	or	%edi,(%rsp)
	.byte	0x0f,0xae,0x14,0x24	# ldmxcsr (%rsp)
	add	$8,%rsp
	xor	%eax,%eax
	ret
	.size	feraiseexcept,.-feraiseexcept

# int fetestexcept(int e)
	.globl	fetestexcept
	.type	fetestexcept,@function
fetestexcept:
	and	$0x3f,%edi
	sub	$8,%rsp
	fnstsw	%ax
	.byte	0x0f,0xae,0x1c,0x24	# stmxcsr (%rsp)
	or	(%rsp),%eax
	and	%edi,%eax
	add	$8,%rsp
	ret
	.size	fetestexcept,.-fetestexcept

# int fegetround(void)
	.globl	fegetround
	.type	fegetround,@function
fegetround:
	sub	$8,%rsp
	fnstcw	(%rsp)
	movzwl	(%rsp),%eax
	and	$0xc00,%eax
	add	$8,%rsp
	ret
	.size	fegetround,.-fegetround

# int fesetround(int r)
	.globl	fesetround
	.type	fesetround,@function
fesetround:
	test	$~0xc00,%edi
	jnz	1f
	sub	$8,%rsp
	fnstcw	(%rsp)
	andw	$~0xc00,(%rsp)
	or	%di,(%rsp)
	fldcw	(%rsp)
	.byte	0x0f,0xae,0x1c,0x24	# stmxcsr (%rsp)
	andl	$~0x6000,(%rsp)
	shl	$3,%edi
	or	%edi,(%rsp)
	.byte	0x0f,0xae,0x14,0x24	# ldmxcsr (%rsp)
	add	$8,%rsp
	xor	%eax,%eax
	ret
1:	mov	$-1,%eax
	ret
	.size	fesetround,.-fesetround

# int fegetenv(fenv_t *env)
	.globl	fegetenv
	.type	fegetenv,@function
fegetenv:
	fnstenv	(%rdi)
	fldenv	(%rdi)			# fnstenv masked all exceptions: undo that
	sub	$8,%rsp
	.byte	0x0f,0xae,0x1c,0x24	# stmxcsr (%rsp)
	mov	(%rsp),%eax
	mov	%eax,28(%rdi)
	add	$8,%rsp
	xor	%eax,%eax
	ret
	.size	fegetenv,.-fegetenv

# int fesetenv(const fenv_t *env); FE_DFL_ENV is (fenv_t *)-1
	.globl	fesetenv
	.type	fesetenv,@function
fesetenv:
	sub	$40,%rsp
	cmp	$-1,%rdi
	jne	1f
	fnstenv	(%rsp)			# keep the instruction pointers
	movw	$0x37f,(%rsp)		# all masked, extended precision, nearest
	movw	$0,4(%rsp)		# no flags
	movw	$0xffff,8(%rsp)		# all registers empty
	movl	$0x1f80,28(%rsp)
	lea	(%rsp),%rdi
1:	fldenv	(%rdi)
	mov	28(%rdi),%eax
	mov	%eax,32(%rsp)
	lea	32(%rsp),%rsp
	.byte	0x0f,0xae,0x14,0x24	# ldmxcsr (%rsp)
	add	$8,%rsp
	xor	%eax,%eax
	ret
	.size	fesetenv,.-fesetenv

	.section .note.GNU-stack,"",@progbits
