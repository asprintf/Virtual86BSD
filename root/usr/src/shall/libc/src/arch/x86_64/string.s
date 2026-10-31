# void *memcpy(void *restrict d, const void *restrict s, size_t n)
	.text
	.globl	memcpy
	.type	memcpy,@function
memcpy:
	mov	%rdi,%rax
	cmp	$8,%rdx
	jb	1f
	test	$7,%edi			# align destination to 8 bytes
	jz	2f
0:	movsb
	dec	%rdx
	test	$7,%edi
	jnz	0b
2:	mov	%rdx,%rcx
	shr	$3,%rcx
	rep movsq
	and	$7,%edx
1:	mov	%rdx,%rcx
	rep movsb
	ret
	.size	memcpy,.-memcpy

# void *memset(void *d, int c, size_t n)
	.globl	memset
	.type	memset,@function
memset:
	mov	%rdi,%r8
	movzbl	%sil,%eax
	mov	$0x0101010101010101,%r9
	imul	%r9,%rax
	cmp	$16,%rdx
	jb	1f
	mov	%rdx,%rcx
	shr	$3,%rcx
	rep stosq
	and	$7,%edx
1:	mov	%rdx,%rcx
	rep stosb
	mov	%r8,%rax
	ret
	.size	memset,.-memset
	.section .note.GNU-stack,"",@progbits
