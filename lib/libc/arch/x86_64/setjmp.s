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

# int setjmp(jmp_buf), void longjmp(jmp_buf, int)
# jmp_buf: rbx, rbp, r12, r13, r14, r15, rsp, rip (as glibc's layout).
	.text
	.globl	setjmp
	.type	setjmp,@function
setjmp:
	mov	%rbx,(%rdi)
	mov	%rbp,8(%rdi)
	mov	%r12,16(%rdi)
	mov	%r13,24(%rdi)
	mov	%r14,32(%rdi)
	mov	%r15,40(%rdi)
	lea	8(%rsp),%rdx
	mov	%rdx,48(%rdi)
	mov	(%rsp),%rdx
	mov	%rdx,56(%rdi)
	xor	%eax,%eax
	ret
	.size	setjmp,.-setjmp

	.globl	longjmp
	.type	longjmp,@function
longjmp:
	xor	%eax,%eax
	cmp	$1,%esi
	adc	%esi,%eax
	mov	(%rdi),%rbx
	mov	8(%rdi),%rbp
	mov	16(%rdi),%r12
	mov	24(%rdi),%r13
	mov	32(%rdi),%r14
	mov	40(%rdi),%r15
	mov	48(%rdi),%rsp
	jmp	*56(%rdi)
	.size	longjmp,.-longjmp
	.section .note.GNU-stack,"",@progbits
