# Copyright (c) 2026, Joost Orchidee Winter
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
