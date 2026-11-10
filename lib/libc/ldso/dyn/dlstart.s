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

# The dynamic linker's entry point (libc.so's ELF entry): the kernel
# leaves the program's argc, argv, envp and auxv on the stack.
	.text
	.globl	_dlstart
	.type	_dlstart,@function
_dlstart:
	xor	%ebp,%ebp
	mov	%rsp,%rdi		# the initial stack
	and	$-16,%rsp
	call	__dls1			# (hidden: no relocation needed)
	hlt
	.size	_dlstart,.-_dlstart

# _Noreturn void __dls_jump(size_t entry, size_t *sp): start the program
# with the stack the kernel made (%rdx: no termination function).
	.globl	__dls_jump
	.hidden	__dls_jump
	.type	__dls_jump,@function
__dls_jump:
	mov	%rsi,%rsp
	xor	%edx,%edx
	jmp	*%rdi
	.size	__dls_jump,.-__dls_jump
	.section .note.GNU-stack,"",@progbits
