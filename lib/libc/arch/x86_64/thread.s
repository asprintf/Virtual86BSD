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

# Thread primitives (see src/internal/pthread_impl.h).
	.text

# long __syscall_cp_asm(volatile int *cancel, long nr, a, b, c, d, e, f)
# A cancellation point: if *cancel is set, or becomes set before the
# system call has started (SIGCANCEL's handler then moves the program
# counter from [__cp_begin, __cp_end) to __cp_cancel), __cancel() is
# called instead.
	.globl	__syscall_cp_asm
	.hidden	__syscall_cp_asm
	.type	__syscall_cp_asm,@function
	.globl	__cp_begin
	.hidden	__cp_begin
	.globl	__cp_end
	.hidden	__cp_end
	.globl	__cp_cancel
	.hidden	__cp_cancel
__syscall_cp_asm:
__cp_begin:
	mov	(%rdi),%eax
	test	%eax,%eax
	jnz	__cp_cancel
	mov	%rsi,%rax
	mov	%rdx,%rdi
	mov	%rcx,%rsi
	mov	%r8,%rdx
	mov	%r9,%r10
	mov	8(%rsp),%r8
	mov	16(%rsp),%r9
	syscall
__cp_end:
	jnc	1f
	neg	%rax
1:	ret
__cp_cancel:
	jmp	__cancel
	.size	__syscall_cp_asm,.-__syscall_cp_asm

# _Noreturn void __unmapself(void *base, size_t size): unmap the stack
# the caller runs on, then exit the thread (thr_exit, no state word),
# touching no memory between.
	.globl	__unmapself
	.hidden	__unmapself
	.type	__unmapself,@function
__unmapself:
	mov	$SYS_munmap,%eax
	syscall
	xor	%edi,%edi
	mov	$SYS_thr_exit,%eax
	syscall
	hlt
	.size	__unmapself,.-__unmapself

	.section .note.GNU-stack,"",@progbits
