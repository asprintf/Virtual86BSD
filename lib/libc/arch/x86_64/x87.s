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

# x87 and SSE primitives for the mathematics library (src/internal/libm.h).
# long double arguments are on the stack at 8(%rsp), 24(%rsp), ...;
# a long double result is returned in %st(0).
	.text

	.globl	__x87_sin
	.hidden	__x87_sin
	.type	__x87_sin,@function
__x87_sin:
	fldt	8(%rsp)
	fsin
	ret
	.size	__x87_sin,.-__x87_sin

	.globl	__x87_cos
	.hidden	__x87_cos
	.type	__x87_cos,@function
__x87_cos:
	fldt	8(%rsp)
	fcos
	ret
	.size	__x87_cos,.-__x87_cos

	.globl	__x87_tan
	.hidden	__x87_tan
	.type	__x87_tan,@function
__x87_tan:
	fldt	8(%rsp)
	fptan
	fstp	%st(0)
	ret
	.size	__x87_tan,.-__x87_tan

# atan2(y = 8(%rsp), x = 24(%rsp))
	.globl	__x87_atan2
	.hidden	__x87_atan2
	.type	__x87_atan2,@function
__x87_atan2:
	fldt	8(%rsp)
	fldt	24(%rsp)
	fpatan
	ret
	.size	__x87_atan2,.-__x87_atan2

# y * log2(x): x = 8(%rsp), y = 24(%rsp)
	.globl	__x87_yl2x
	.hidden	__x87_yl2x
	.type	__x87_yl2x,@function
__x87_yl2x:
	fldt	24(%rsp)
	fldt	8(%rsp)
	fyl2x
	ret
	.size	__x87_yl2x,.-__x87_yl2x

	.globl	__x87_yl2xp1
	.hidden	__x87_yl2xp1
	.type	__x87_yl2xp1,@function
__x87_yl2xp1:
	fldt	24(%rsp)
	fldt	8(%rsp)
	.byte	0xd9,0xf9		# fyl2xp1 (spelled out for assemblers without it)
	ret
	.size	__x87_yl2xp1,.-__x87_yl2xp1

	.globl	__x87_2xm1
	.hidden	__x87_2xm1
	.type	__x87_2xm1,@function
__x87_2xm1:
	fldt	8(%rsp)
	f2xm1
	ret
	.size	__x87_2xm1,.-__x87_2xm1

# x * 2^trunc(n): x = 8(%rsp), n = 24(%rsp)
	.globl	__x87_scale
	.hidden	__x87_scale
	.type	__x87_scale,@function
__x87_scale:
	fldt	24(%rsp)
	fldt	8(%rsp)
	fscale
	fstp	%st(1)
	ret
	.size	__x87_scale,.-__x87_scale

	.globl	__x87_sqrt
	.hidden	__x87_sqrt
	.type	__x87_sqrt,@function
__x87_sqrt:
	fldt	8(%rsp)
	fsqrt
	ret
	.size	__x87_sqrt,.-__x87_sqrt

	.globl	__x87_rint
	.hidden	__x87_rint
	.type	__x87_rint,@function
__x87_rint:
	fldt	8(%rsp)
	frndint
	ret
	.size	__x87_rint,.-__x87_rint

# fmod(x = 8(%rsp), y = 24(%rsp), int *q = %rdi): fprem until complete;
# *q = the low three bits of the quotient (C0 = Q2, C3 = Q1, C1 = Q0).
	.globl	__x87_fmod
	.hidden	__x87_fmod
	.type	__x87_fmod,@function
__x87_fmod:
	fldt	24(%rsp)
	fldt	8(%rsp)
1:	fprem
	fnstsw	%ax
	testw	$0x400,%ax
	jnz	1b
	jmp	__x87_qbits
	.size	__x87_fmod,.-__x87_fmod

	.globl	__x87_remainder
	.hidden	__x87_remainder
	.type	__x87_remainder,@function
__x87_remainder:
	fldt	24(%rsp)
	fldt	8(%rsp)
1:	fprem1
	fnstsw	%ax
	testw	$0x400,%ax
	jnz	1b
	jmp	__x87_qbits
	.size	__x87_remainder,.-__x87_remainder

# common tail: drop y, store the quotient bits from the status word in %ax
__x87_qbits:
	fstp	%st(1)
	xor	%ecx,%ecx
	testw	$0x100,%ax
	jz	1f
	or	$4,%ecx
1:	testw	$0x4000,%ax
	jz	2f
	or	$2,%ecx
2:	testw	$0x200,%ax
	jz	3f
	or	$1,%ecx
3:	mov	%ecx,(%rdi)
	ret

	.globl	__sse_sqrt
	.hidden	__sse_sqrt
	.type	__sse_sqrt,@function
__sse_sqrt:
	sqrtsd	%xmm0,%xmm0
	ret
	.size	__sse_sqrt,.-__sse_sqrt

	.globl	__sse_sqrtf
	.hidden	__sse_sqrtf
	.type	__sse_sqrtf,@function
__sse_sqrtf:
	sqrtss	%xmm0,%xmm0
	ret
	.size	__sse_sqrtf,.-__sse_sqrtf

# signgam (XSI): lgamma() sets __signgam, of which signgam is a weak
# alias, so that the ISO C functions use no name outside the ISO namespace.
	.bss
	.globl	__signgam
	.type	__signgam,@object
	.size	__signgam,4
	.align	4
__signgam:
	.zero	4
	.weak	signgam
	.set	signgam,__signgam

	.section .note.GNU-stack,"",@progbits
