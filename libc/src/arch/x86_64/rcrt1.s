# Process entry point of a static PIE (gcc -static-pie): the program
# relocates itself first (R_X86_64_RELATIVE, and DT_RELR), with nothing
# that needs a relocation, then starts as crt1.s.
	.text
	.globl	_start
	.type	_start,@function
_start:
	xor	%ebp,%ebp
	mov	%rsp,%r12		# the initial stack (callee-saved)
	lea	__ehdr_start(%rip),%rdi	# the load address
	lea	_DYNAMIC(%rip),%rsi
	xor	%ecx,%ecx		# rela
	xor	%edx,%edx		# relasz
	xor	%r8d,%r8d		# relr
	xor	%r9d,%r9d		# relrsz
1:	mov	(%rsi),%rax
	test	%rax,%rax
	jz	5f
	mov	8(%rsi),%r10
	cmp	$7,%rax			# DT_RELA
	jne	2f
	lea	(%rdi,%r10),%rcx
2:	cmp	$8,%rax			# DT_RELASZ
	jne	3f
	mov	%r10,%rdx
3:	cmp	$36,%rax		# DT_RELR
	jne	4f
	lea	(%rdi,%r10),%r8
4:	cmp	$35,%rax		# DT_RELRSZ
	jne	.Lnext
	mov	%r10,%r9
.Lnext:	add	$16,%rsi
	jmp	1b
5:	test	%rcx,%rcx		# R_X86_64_RELATIVE: *(base + off) = base + addend
	jz	8f
	add	%rcx,%rdx		# end
6:	cmp	%rdx,%rcx
	jae	8f
	cmpl	$8,8(%rcx)
	jne	7f
	mov	(%rcx),%rax
	mov	16(%rcx),%r10
	add	%rdi,%r10
	mov	%r10,(%rdi,%rax)
7:	add	$24,%rcx
	jmp	6b
8:	test	%r8,%r8			# DT_RELR
	jz	.Lrelr_done
	add	%r8,%r9			# end
	xor	%r11d,%r11d		# where
9:	cmp	%r9,%r8
	jae	.Lrelr_done
	mov	(%r8),%rax
	test	$1,%rax
	jnz	.Lbitmap
	lea	(%rdi,%rax),%r11	# an address
	add	%rdi,(%r11)
	add	$8,%r11
	jmp	.Lrelr_next
.Lbitmap:	shr	$1,%rax			# a bitmap of the 63 words from where
	mov	%r11,%r10
.Lbits:	test	%rax,%rax
	jz	.Lbits_done
	test	$1,%rax
	jz	.Lbit_next
	add	%rdi,(%r10)
.Lbit_next:	add	$8,%r10
	shr	$1,%rax
	jmp	.Lbits
.Lbits_done:	add	$63*8,%r11
.Lrelr_next:	add	$8,%r8
	jmp	9b
.Lrelr_done:	mov	%r12,%rdi
	lea	main(%rip),%rsi
	and	$-16,%rsp
	call	__libc_start_main
	hlt
	.size	_start,.-_start
	.section .note.GNU-stack,"",@progbits
