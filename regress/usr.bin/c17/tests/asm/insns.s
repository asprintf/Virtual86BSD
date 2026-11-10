	.text
	.globl f
f:
	push %rbp
	mov %rsp, %rbp
	sub $16, %rsp
	sub $4096, %rsp
	mov %rdi, -8(%rbp)
	mov -8(%rbp), %rax
	movl $5, -4(%rbp)
	movq $-1, (%rax)
	movb $3, (%rdi,%rsi,4)
	movw $300, 8(%r12)
	mov $0x123456789, %rax
	mov $-5, %rax
	mov $5, %eax
	mov $5, %r9d
	mov $1, %r10
	movabs $1, %rax
	mov %al, (%rsp)
	mov %sil, %dil
	mov %r8b, (%r13)
	mov (%rbp), %rax
	mov (%r13), %rax
	mov (%r12), %rax
	mov 128(%rax), %rcx
	mov -128(%rax), %rcx
	mov 0x1000(,%rax,8), %rcx
	movzbl %al, %eax
	movzbl (%rax), %ecx
	movsbq %dil, %rax
	movswl (%rax), %eax
	movslq %eax, %rax
	movslq (%rdi), %r11
	movzwl %ax, %eax
	lea 8(%rax,%rbx,2), %rcx
	lea x(%rip), %rax
	add %rdi, %rax
	add $1, %rax
	add $1000, %eax
	add $1000, %rax
	addl $1, (%rax)
	addq $200, 8(%rsp)
	and $-16, %rsp
	or %eax, %ecx
	xor %eax, %eax
	xor %r8d, %r8d
	cmp %rdi, %rsi
	cmp $0, %eax
	cmpl $0, -4(%rbp)
	cmpb $10, (%rax)
	cmp $5, %al
	cmp $5, %cl
	test %rax, %rax
	test %al, %al
	test $1, %al
	test $1, %ecx
	test $256, %eax
	neg %rax
	not %ecx
	imul %rdi, %rax
	imul $10, %rax, %rax
	imul $1000, %rcx, %rdx
	idiv %rcx
	idivl -4(%rbp)
	div %rdi
	mul %rcx
	cqo
	cltd
	cltq
	shl %cl, %rax
	shl $3, %rax
	shl $1, %eax
	sar %cl, %edi
	shr $63, %rax
	sarq $2, (%rax)
	sete %al
	setne %cl
	setl %dil
	setae %r9b
	cmove %rcx, %rax
	cmovl %esi, %edi
	push %r12
	pop %r12
	push $1
	push $1000
	pushq (%rax)
	xchg %rax, (%rdi)
	xchg %eax, %ecx
	lock cmpxchg %rcx, (%rdi)
	lock cmpxchgl %ecx, (%rdi)
	lock xadd %eax, (%rdx)
	mfence
	rep movsb
	rep stosb
	btc $63, %rax
	bt %rax, %rdx
	bswap %eax
	bswap %r10
	bsr %rax, %rcx
	bsf %edi, %eax
	inc %eax
	decq (%rax)
	call *%rax
	call *8(%rax)
	jmp *%rcx
	jmp *(%rax,%rcx,8)
	syscall
	movl %fs:0, %eax
	movq %fs:0, %rax
	mov %fs:t@tpoff, %eax
	movq t@gottpoff(%rip), %rax
	movss (%rax), %xmm0
	movsd %xmm1, 8(%rsp)
	movsd %xmm9, %xmm0
	movss %xmm0, %xmm15
	movaps %xmm0, %xmm1
	movq %xmm0, %rax
	movq %rax, %xmm0
	movd %xmm0, %eax
	movd %eax, %xmm1
	movq (%rax), %xmm0
	movq %xmm0, (%rax)
	addsd %xmm1, %xmm0
	addss (%rax), %xmm0
	subsd %xmm1, %xmm0
	mulsd %xmm8, %xmm2
	divss %xmm1, %xmm0
	ucomisd %xmm1, %xmm0
	ucomiss %xmm1, %xmm0
	xorps %xmm0, %xmm0
	xorpd %xmm1, %xmm1
	pxor %xmm2, %xmm2
	cvtsi2sd %eax, %xmm0
	cvtsi2sdq %rax, %xmm0
	cvtsi2ssl %eax, %xmm0
	cvtsi2ssq %rax, %xmm0
	cvttsd2si %xmm0, %eax
	cvttsd2siq %xmm0, %rax
	cvttss2sil %xmm0, %eax
	cvtss2sd %xmm0, %xmm0
	cvtsd2ss %xmm0, %xmm1
	sqrtsd %xmm0, %xmm0
	fldt (%rax)
	fldl 8(%rsp)
	flds (%rax)
	fstpt (%rdi)
	fstpl (%rax)
	fstps (%rax)
	fildl (%rax)
	fildll (%rax)
	fildq (%rax)
	fistpl (%rax)
	fistpll (%rax)
	fistps (%rax)
	fnstcw (%rax)
	fldcw (%rax)
	faddp
	fmulp
	fsubrp
	fdivrp
	fsubp
	fdivp
	faddp %st, %st(1)
	fsub %st(1), %st
	fucomip %st(1), %st
	fucomi %st(1), %st
	fxch %st(1)
	fstp %st(1)
	fstp %st(0)
	fld %st(0)
	fldz
	fchs
	fadds (%rax)
	leave
	ret
	.data
x:	.long 1, 2
	.quad f
	.quad x+8
	.byte 1, 2, 3
	.zero 5
	.ascii "ab\n"
	.asciz "c\"d\\"
	.balign 8
	.value 7
	.section .rodata
	.long f-.
	.section .tbss,"awT",@nobits
t:	.zero 4
	.text
	.globl g
	.type g, @function
g:
	movq $60, %rax
	movl $1, %edi
	syscall
	movq %rdi, %rax
	movq %rsi, %rdi
	movq %rdx, %rsi
	mov %r10, %rcx
	movq 8(%rsp), %r8
	mov %fs:0, %rax
	lea -16(%rax), %rcx
	movzbl (%rdi,%rcx), %eax
	sub %r11, %rax
	xorl %r9d, %r9d
	movabsq $0x7fffffffffffffff, %r14
	cmpq $-129, %r15
	andl $0xff, %r13d
	orq $0x80, (%r12,%r13,8)
	adcq $0, %rdx
	sbbl %eax, %eax
	testb $0x80, 3(%rbp)
	notq %r8
	negl (%rsp)
	mull %ecx
	imulq (%rsi), %rdi
	imull $-3, 4(%rbx), %eax
	divq 8(%rsp)
	shrl %cl, 4(%rax)
	rolq $5, %r9
	rorw $1, %ax
	sall $2, %r10d
	shlb %cl, %bl
	movb %ah, %al
	movb %bh, (%rdi)
	movw %si, 6(%rdx)
	movw (%rsi), %r9w
	cwtl
	cbtw
	cqto
	pushq %r15
	popq %rbx
	pushq $-1
	incq %r12
	decb (%rdi)
	xaddq %rax, (%rsi)
	cmpxchgq %rcx, 8(%rdi)
	lock orl $0, (%rsp)
	btq $3, (%rdi)
	btsq %rax, %rdx
	btrl $31, %ecx
	popcntq %rdi, %rax
	tzcntl %esi, %eax
	lzcntq %r8, %r9
	cmovaq %r8, %r9
	cmovbe (%rdi), %ecx
	setg %r12b
	setp 7(%rsp)
	rep stosq
	rep movsq
	cld
	hlt
	ud2
	pause
	cpuid
	rdtsc
	movups (%rdi), %xmm8
	movups %xmm8, (%rsi)
	movdqu (%rdi), %xmm1
	movdqa %xmm1, 16(%rsp)
	pand %xmm2, %xmm3
	por %xmm4, %xmm5
	andpd %xmm0, %xmm1
	andnps %xmm2, %xmm3
	orpd %xmm6, %xmm7
	maxsd %xmm1, %xmm2
	minss (%rax), %xmm3
	sqrtss %xmm4, %xmm5
	comisd %xmm1, %xmm2
	cvtsd2si %xmm0, %rax
	cvtss2si %xmm1, %ecx
	cvttss2si %xmm2, %r8
	unpcklps %xmm1, %xmm2
	fld1
	fabs
	fsqrt
	frndint
	fprem
	fistl (%rax)
	fisttpll (%rax)
	fnstsw %ax
	fucomp %st(1)
	fucompp
	fcomip %st(1), %st
	ffree %st(2)
	fadd %st(2), %st
	fmul %st, %st(3)
	fdivr %st(1), %st
	fdiv %st, %st(2)
	fsubr %st, %st(1)
	fninit
	fwait
	int3
	int $0x80
	ret $8
	.size g, .-g
	.section .data.rel.ro,"aw"
	.quad g+16
	.long 0x12345678
	.short -2
	.byte 'a', '\n'
	.string "str"
	.p2align 3
	.8byte 1
	.4byte 2
	.2byte 3
	.skip 3, 0xaa
	.set five, 5
	.long five*2+1
	.long (1<<4)|3
