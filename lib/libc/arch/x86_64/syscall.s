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

# long __syscall6(long n, long a, long b, long c, long d, long e, long f)
# Raw system call; returns the kernel's result (-errno on failure).
	.text
	.globl	__syscall6
	.hidden	__syscall6
	.type	__syscall6,@function
__syscall6:
	mov	%rdi,%rax
	mov	%rsi,%rdi
	mov	%rdx,%rsi
	mov	%rcx,%rdx
	mov	%r8,%r10
	mov	%r9,%r8
	mov	8(%rsp),%r9
	syscall
	ret
	.size	__syscall6,.-__syscall6

# struct __pthread *__get_tp(void): the thread pointer (%fs:0 holds it).
	.globl	__get_tp
	.hidden	__get_tp
	.type	__get_tp,@function
__get_tp:
	mov	%fs:0,%rax
	ret
	.size	__get_tp,.-__get_tp

# void __restore_rt(void): signal-return trampoline for SA_RESTORER.
	.globl	__restore_rt
	.hidden	__restore_rt
	.type	__restore_rt,@function
__restore_rt:
	mov	$SYS_rt_sigreturn,%eax
	syscall
	.size	__restore_rt,.-__restore_rt
	.section .note.GNU-stack,"",@progbits
