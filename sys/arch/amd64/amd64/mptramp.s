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

# mptramp.s - how another processor gets from the start-up IPI to the kernel.
# It starts in real mode at the page cpu_boot copied ap_tramp_start to (below
# 1 MiB), enters protected mode there, runs ap_start32 (here in the image, by
# its physical address) which turns on long mode with the page tables ap_pml4,
# and ap_start64 jumps to the higher half and calls ap_main on the stack and
# with the cpu_info cpu_boot left in ap_boot_rsp and ap_boot_ci.

	.set KERNEL_BASE, 0xffffffff80000000

	.text
	.globl ap_tramp_start, ap_tramp_end, ap_tramp_gdt, ap_tramp_gdtptr
ap_tramp_start:
	.byte 0xfa			# cli
	.byte 0x8c, 0xc8		# movw %cs, %ax
	.byte 0x8e, 0xd8		# movw %ax, %ds
	.byte 0x66, 0x0f, 0x01, 0x16	# lgdtl ap_tramp_gdtptr
	.word ap_tramp_gdtptr - ap_tramp_start
	.byte 0x66, 0xb8		# movl $0x11, %eax	(PE, ET: nothing else, caches on)
	.long 0x11
	.byte 0x0f, 0x22, 0xc0		# movl %eax, %cr0
	.byte 0x66, 0xea		# ljmpl $0x08, $ap_start32
	.long ap_start32 - KERNEL_BASE
	.word 0x08

	.balign 8
ap_tramp_gdt:
	.quad 0
	.quad 0x00cf9a000000ffff	# 0x08: 32-bit code
	.quad 0x00af9a000000ffff	# 0x10: 64-bit code
	.quad 0x00cf92000000ffff	# 0x18: data
ap_tramp_gdtptr:
	.word ap_tramp_gdtptr - ap_tramp_gdt - 1
	.long 0				# (cpu_boot: the physical address of ap_tramp_gdt in its page)
ap_tramp_end:

	.code32
ap_start32:
	movw $0x18, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movl $(ap_pml4 - KERNEL_BASE), %eax
	movl %eax, %cr3
	movl (ap_cr4 - KERNEL_BASE), %eax
	movl %eax, %cr4
	movl $0xC0000080, %ecx		# EFER: LME | NXE | SCE
	rdmsr
	orl $0x901, %eax
	wrmsr
	movl (ap_cr0 - KERNEL_BASE), %eax
	movl %eax, %cr0
	lgdt (boot_gdt_ptr - KERNEL_BASE)
	ljmp $0x10, $(ap_start64_low - KERNEL_BASE)

	.code64
ap_start64_low:
	movabsq $ap_start64, %rax
	jmp *%rax

ap_start64:
	movw $0x18, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	xorw %ax, %ax
	movw %ax, %fs
	movw %ax, %gs
	movq ap_boot_rsp(%rip), %rsp
	movq ap_boot_ci(%rip), %rdi
	xorl %ebp, %ebp
	call ap_main
1:	cli
	hlt
	jmp 1b

	.bss
	.balign 4096
	.globl ap_pml4
ap_pml4:	.zero 4096
	.globl ap_boot_rsp, ap_boot_ci, ap_cr0, ap_cr4
ap_boot_rsp:	.zero 8
ap_boot_ci:	.zero 8
ap_cr0:		.zero 8
ap_cr4:		.zero 8
