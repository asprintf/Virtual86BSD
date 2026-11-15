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

/* mbr.S - stage 1 of the loader: the boot code in the first 440 bytes of
   sector 0 of the disk (an MBR, or the protective MBR of a GPT).  The BIOS
   loads it at 0:7c00 with the boot drive in %dl; it reads stage 2 (NSECT
   sectors from LBA 34, which both an MBR and a GPT disk leave free) to
   0:8000 with the BIOS's extended read and jumps there. */

	.set BOOTLBA, 34
	.set BOOTADDR, 0x8000

	.text
	.code16
	.globl _start
_start:
	cli
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0x7c00, %sp
	sti
	cld
	movb %dl, drive

	movb $0x41, %ah                 # extended read present?
	movw $0x55aa, %bx
	int $0x13
	jc fail
	cmpw $0xaa55, %bx
	jne fail

	movw $NSECT, left
more:	movw $dap, %si
	movw left, %cx
	cmpw $32, %cx
	jbe 1f
	movw $32, %cx
1:	movw %cx, chunk
	movw %cx, 2(%si)
	movb $3, tries
retry:	movb $0x42, %ah
	movb drive, %dl
	int $0x13
	jnc ok
	xorw %ax, %ax                   # reset the disk, try again
	movb drive, %dl
	int $0x13
	decb tries
	jnz retry
	jmp fail
ok:	movzwl chunk, %ecx
	subw %cx, left
	jz go
	addl %ecx, 8(%si)               # the next sectors
	adcl $0, 12(%si)
	shlw $5, %cx                    # 512 bytes = 32 paragraphs each
	addw %cx, 6(%si)
	jmp more
go:	movb drive, %dl
	ljmp $0, $BOOTADDR

fail:	movw $msg, %si
1:	lodsb
	testb %al, %al
	jz 2f
	movb $0x0e, %ah
	movw $7, %bx
	int $0x10
	jmp 1b
2:	hlt
	jmp 2b

msg:	.asciz "boot: cannot read the loader\r\n"
drive:	.byte 0
tries:	.byte 0
left:	.word 0
chunk:	.word 0
dap:	.byte 16, 0
	.word 0                         # sectors
	.word 0, BOOTADDR >> 4          # buffer: offset 0, segment 0x800
	.long BOOTLBA, 0

	.org 440
