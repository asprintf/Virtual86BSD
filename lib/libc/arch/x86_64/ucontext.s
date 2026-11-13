# Copyright (c) 2026, asprintf
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

# getcontext, setcontext, swapcontext: SUSv2, SUSv3.  setcontext builds a
# signal frame and calls rt_sigreturn, so signal handler contexts work too.

	.set	UC_STACK, 16
	.set	G, 40			# uc_mcontext's general registers
	.set	FPREGS, 224
	.set	MASK, 296
	.set	FPMEM, 424

# save the caller's context into %rdi's ucontext_t (return address at
# off(%rsp)); it resumes returning 0; %r8 holds the ucontext_t after
	.macro	SAVE off
	mov	%r8,G+0(%rdi)
	mov	%r9,G+8(%rdi)
	mov	%r10,G+16(%rdi)
	mov	%r11,G+24(%rdi)
	mov	%r12,G+32(%rdi)
	mov	%r13,G+40(%rdi)
	mov	%r14,G+48(%rdi)
	mov	%r15,G+56(%rdi)
	mov	%rdi,G+64(%rdi)
	mov	%rsi,G+72(%rdi)
	mov	%rbp,G+80(%rdi)
	mov	%rbx,G+88(%rdi)
	mov	%rdx,G+96(%rdi)
	movq	$0,G+104(%rdi)		# rax: the function returns 0
	mov	%rcx,G+112(%rdi)
	lea	\off+8(%rsp),%rax
	mov	%rax,G+120(%rdi)	# rsp: after the return
	mov	\off(%rsp),%rax
	mov	%rax,G+128(%rdi)	# rip: the return address
	pushfq
	pop	%rax
	mov	%rax,G+136(%rdi)
	movabs	$0x002b000000000033,%rax
	mov	%rax,G+144(%rdi)	# cs (user code), gs, fs, ss (user data)
	xor	%eax,%eax
	mov	%rax,G+152(%rdi)	# err, trapno, oldmask, cr2
	mov	%rax,G+160(%rdi)
	mov	%rax,G+168(%rdi)
	mov	%rax,G+176(%rdi)
	mov	%rax,(%rdi)		# uc_flags
	mov	%rax,8(%rdi)		# uc_link: none
	mov	%rdi,%r8
	# FPU state, via an aligned buffer
	mov	%rsp,%r9
	sub	$512,%rsp
	and	$-16,%rsp
	fxsave	(%rsp)
	mov	%rsp,%rsi
	lea	FPMEM(%r8),%rdi
	mov	$64,%ecx
	rep	movsq
	mov	%r9,%rsp
	lea	FPMEM(%r8),%rax
	mov	%rax,FPREGS(%r8)
	xor	%eax,%eax
	lea	FPREGS+8(%r8),%rdi	# the reserved words
	mov	$8,%ecx
	rep	stosq
	# the signal mask
	lea	MASK(%r8),%rdi
	mov	$16,%ecx
	rep	stosq
	xor	%edi,%edi
	xor	%esi,%esi
	lea	MASK(%r8),%rdx
	mov	$8,%r10d
	mov	$SYS_rt_sigprocmask,%eax
	syscall
	# uc_stack: the alternate signal stack
	xor	%edi,%edi
	lea	UC_STACK(%r8),%rsi
	mov	$SYS_sigaltstack,%eax
	syscall
	.endm

	.text
	.globl	getcontext
	.type	getcontext,@function
getcontext:
	SAVE	0
	xor	%eax,%eax
	ret
	.size	getcontext,.-getcontext

	.globl	swapcontext
	.type	swapcontext,@function
swapcontext:
	push	%rsi
	SAVE	8
	pop	%rdi
	jmp	setcontext
	.size	swapcontext,.-swapcontext

	.globl	setcontext
	.type	setcontext,@function
setcontext:
	mov	%rdi,%r8
	sub	$1024,%rsp
	and	$-64,%rsp
	# (%rsp): FPU image, 512(%rsp): the frame
	mov	FPREGS(%r8),%rsi
	xor	%r9d,%r9d
	test	%rsi,%rsi
	jz	1f
	mov	%rsp,%rdi
	mov	$64,%ecx
	rep	movsq
	mov	%rsp,%r9
1:	lea	520(%rsp),%rdi
	mov	%r8,%rsi
	mov	$38,%ecx
	rep	movsq
	mov	%r9,520+FPREGS(%rsp)
	xor	%edi,%edi
	lea	520+UC_STACK(%rsp),%rsi
	mov	$SYS_sigaltstack,%eax
	syscall
	lea	520(%rsp),%rsp
	mov	$SYS_rt_sigreturn,%eax
	syscall
	ud2				# the kernel refused the frame: SIGSEGV
	.size	setcontext,.-setcontext

# makecontext's functions return here: resume uc_link (%rbx) or exit the thread
	.globl	__start_context
	.hidden	__start_context
	.type	__start_context,@function
__start_context:
	mov	%rbx,%rdi
	test	%rdi,%rdi
	jz	1f
	call	setcontext
1:	xor	%edi,%edi
	call	pthread_exit
	ud2
	.size	__start_context,.-__start_context
	.section .note.GNU-stack,"",@progbits
