/*
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* start.S - stage 2 of the loader: the 16-bit entry stage 1 jumps to, the
   switch to 32-bit protected mode (flat, 4 GiB), and bioscall(), which
   runs a BIOS interrupt by going back to real mode and again to protected
   mode.  Everything that real mode must reach (this code, the registers,
   the buffers of the BIOS) is in the first 64 KiB: .text16 and .data16
   come first in the image (boot.ld). */

	.set STACK, 0x7c00
	.set CODE32, 0x08
	.set DATA32, 0x10
	.set CODE16, 0x18
	.set DATA16, 0x20

	.section .text16, "ax"
	.code16
	.globl _start
_start:
	cli
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movl $STACK, %esp
	movb %dl, bootdrive

	inb $0x92, %al                  # the A20 gate, by the fast one
	testb $2, %al
	jnz 1f
	orb $2, %al
	andb $0xfe, %al
	outb %al, $0x92
1:	lgdt gdtdesc
	movl %cr0, %eax
	orl $1, %eax
	movl %eax, %cr0
	ljmpl $CODE32, $start32

	.code32
start32:
	movw $DATA32, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	movl $STACK, %esp
	cld
	movl $__bss_start, %edi
	movl $__bss_end, %ecx
	subl %edi, %ecx
	xorl %eax, %eax
	rep stosb
	movzbl bootdrive, %eax
	pushl %eax
	call main
1:	hlt
	jmp 1b

/* void bioscall(int intno, struct biosregs *r): the BIOS interrupt intno
   with the registers of *r, which gets the registers it returns */
	.globl bioscall
bioscall:
	pushl %ebp
	movl %esp, %ebp
	pushal
	movl 8(%ebp), %eax
	movb %al, intins + 1
	movl 12(%ebp), %esi
	movl %esi, caller
	movl $regs, %edi
	movl $(REGSSIZE / 4), %ecx
	cld
	rep movsl
	movl %esp, savedesp
	cli
	ljmp $CODE16, $rm16

	.code16
rm16:
	movw $DATA16, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	movl %cr0, %eax
	andl $0xfffffffe, %eax
	movl %eax, %cr0
	ljmp $0, $rm
rm:
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	movl savedesp, %esp
	lidt rmidt
	movl r_ebx, %ebx
	movl r_ecx, %ecx
	movl r_edx, %edx
	movl r_esi, %esi
	movl r_edi, %edi
	movl r_ebp, %ebp
	movw r_es, %es
	movl r_eax, %eax
	sti
intins:	int $0
	cli
	movl %eax, %cs:r_eax
	movl %ebx, %cs:r_ebx
	movl %ecx, %cs:r_ecx
	movl %edx, %cs:r_edx
	movl %esi, %cs:r_esi
	movl %edi, %cs:r_edi
	movl %ebp, %cs:r_ebp
	movw %es, %cs:r_es
	pushfw
	popw %cs:r_flags
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movl %cr0, %eax
	orl $1, %eax
	movl %eax, %cr0
	ljmpl $CODE32, $pm

	.code32
pm:
	movw $DATA32, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	movl savedesp, %esp
	movl $regs, %esi
	movl caller, %edi
	movl $(REGSSIZE / 4), %ecx
	cld
	rep movsl
	popal
	popl %ebp
	ret

	.section .data16, "aw"
	.balign 4
gdt:	.quad 0
	.quad 0x00cf9a000000ffff        # CODE32: 4 GiB
	.quad 0x00cf92000000ffff        # DATA32
	.quad 0x00009a000000ffff        # CODE16: 64 KiB
	.quad 0x000092000000ffff        # DATA16
gdtdesc:
	.word gdtdesc - gdt - 1
	.long gdt
rmidt:	.word 0x3ff
	.long 0
bootdrive:
	.long 0
caller:	.long 0
savedesp:
	.long 0
	.set REGSSIZE, 32
regs:				# struct biosregs
r_eax:	.long 0
r_ebx:	.long 0
r_ecx:	.long 0
r_edx:	.long 0
r_esi:	.long 0
r_edi:	.long 0
r_ebp:	.long 0
r_es:	.word 0
r_flags: .word 0
