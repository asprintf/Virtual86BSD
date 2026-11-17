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

# getcontext, setcontext, swapcontext: SUSv2, SUSv3.  The kernel has them
# (getcontext, setcontext and swapcontext system calls, FreeBSD's mcontext_t);
# an error comes back as -1 with errno set.
	.text
	.globl	getcontext
	.type	getcontext,@function
getcontext:
	mov	$SYS_getcontext,%eax
	syscall
	jc	1f
	ret
1:	mov	%rax,%rdi
	neg	%rdi
	jmp	__syscall_ret
	.size	getcontext,.-getcontext

	.text
	.globl	setcontext
	.type	setcontext,@function
setcontext:
	mov	$SYS_setcontext,%eax
	syscall
	jc	1f
	ret
1:	mov	%rax,%rdi
	neg	%rdi
	jmp	__syscall_ret
	.size	setcontext,.-setcontext

	.text
	.globl	swapcontext
	.type	swapcontext,@function
swapcontext:
	mov	$SYS_swapcontext,%eax
	syscall
	jc	1f
	ret
1:	mov	%rax,%rdi
	neg	%rdi
	jmp	__syscall_ret
	.size	swapcontext,.-swapcontext

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
