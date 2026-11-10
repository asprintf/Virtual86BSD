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

# int sigsetjmp(sigjmp_buf, int savemask), void siglongjmp(sigjmp_buf, int)
# sigjmp_buf: the jmp_buf registers (64 bytes), the flag at 64, the
# signal mask at 72 (8 bytes as the kernel's).
	.text
	.globl	sigsetjmp
	.type	sigsetjmp,@function
sigsetjmp:
	mov	%esi,64(%rdi)
	test	%esi,%esi
	jz	1f
	mov	%rdi,%r9
	mov	$SYS_rt_sigprocmask,%eax	# rt_sigprocmask(SIG_BLOCK, 0, &env->mask, 8)
	xor	%edi,%edi
	xor	%esi,%esi
	lea	72(%r9),%rdx
	mov	$8,%r10d
	syscall
	mov	%r9,%rdi
1:	jmp	setjmp
	.size	sigsetjmp,.-sigsetjmp

	.globl	siglongjmp
	.type	siglongjmp,@function
siglongjmp:
	cmpl	$0,64(%rdi)
	je	1f
	mov	%rdi,%r8
	mov	%esi,%r9d
	mov	$SYS_rt_sigprocmask,%eax	# rt_sigprocmask(SIG_SETMASK, &env->mask, 0, 8)
	mov	$2,%edi
	lea	72(%r8),%rsi
	xor	%edx,%edx
	mov	$8,%r10d
	syscall
	mov	%r8,%rdi
	mov	%r9d,%esi
1:	jmp	longjmp
	.size	siglongjmp,.-siglongjmp
	.section .note.GNU-stack,"",@progbits
