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

# entry.s - interrupt, exception and system call entry; context switch;
# user memory copies with fault recovery.
#
# Every entry builds the same frame (struct regs in include/arch.h), lowest
# address first:
#   r15 r14 r13 r12 rbp rbx r11 r10 r9 r8 rax rcx rdx rsi rdi
#   vector err  rip cs rflags rsp ss
# For a system call, vector is 0x100 and err holds the system call number
# (Linux's orig_rax).  All returns to user mode go through trap_return and
# iretq.

	.text

# stubs: vectors without an error code push a 0 first
	.macro STUB_NOERR n
	.globl isr\n
isr\n:	pushq $0
	pushq $\n
	jmp isr_common
	.endm
	.macro STUB_ERR n
	.globl isr\n
isr\n:	pushq $\n
	jmp isr_common
	.endm
# NMI, double fault and machine check can arrive with either %gs base loaded
# (just before or after a swapgs): isr_paranoid looks at the base itself
	.macro STUB_PARANOID_NOERR n
	.globl isr\n
isr\n:	pushq $0
	pushq $\n
	jmp isr_paranoid
	.endm
	.macro STUB_PARANOID_ERR n
	.globl isr\n
isr\n:	pushq $\n
	jmp isr_paranoid
	.endm

	STUB_NOERR 0
	STUB_NOERR 1
	STUB_PARANOID_NOERR 2
	STUB_NOERR 3
	STUB_NOERR 4
	STUB_NOERR 5
	STUB_NOERR 6
	STUB_NOERR 7
	STUB_PARANOID_ERR 8
	STUB_NOERR 9
	STUB_ERR 10
	STUB_ERR 11
	STUB_ERR 12
	STUB_ERR 13
	STUB_ERR 14
	STUB_NOERR 15
	STUB_NOERR 16
	STUB_ERR 17
	STUB_PARANOID_NOERR 18
	STUB_NOERR 19
	STUB_NOERR 20
	STUB_ERR 21
	STUB_NOERR 22
	STUB_NOERR 23
	STUB_NOERR 24
	STUB_NOERR 25
	STUB_NOERR 26
	STUB_NOERR 27
	STUB_NOERR 28
	STUB_NOERR 29
	STUB_ERR 30
	STUB_NOERR 31
	STUB_NOERR 32
	STUB_NOERR 33
	STUB_NOERR 34
	STUB_NOERR 35
	STUB_NOERR 36
	STUB_NOERR 37
	STUB_NOERR 38
	STUB_NOERR 39
	STUB_NOERR 40
	STUB_NOERR 41
	STUB_NOERR 42
	STUB_NOERR 43
	STUB_NOERR 44
	STUB_NOERR 45
	STUB_NOERR 46
	STUB_NOERR 47
	STUB_NOERR 255
	STUB_NOERR 240
	STUB_NOERR 241
	STUB_NOERR 242
	STUB_NOERR 243
	STUB_NOERR 244
	STUB_NOERR 245
	STUB_NOERR 246
	STUB_NOERR 247
	STUB_NOERR 248
	STUB_NOERR 249
	STUB_NOERR 250
	STUB_NOERR 251
	STUB_NOERR 252
	STUB_NOERR 253
	STUB_NOERR 254

	.data
	.globl isr_table
isr_table:
	.quad isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
	.quad isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
	.quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
	.quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	.quad isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
	.quad isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
	.globl isr_ipi_table
isr_ipi_table:
	.quad isr240, isr241, isr242, isr243, isr244, isr245, isr246, isr247
	.quad isr248, isr249, isr250, isr251, isr252, isr253, isr254
	.text

# (cs of the interrupted code is at 24(%rsp): from user mode, swap in the
# kernel %gs base)
isr_common:
	testb $3, 24(%rsp)
	jz isr_save
	swapgs
isr_save:
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	cld
	movq %rsp, %rdi
	call trap_handler
	.globl trap_return
trap_return:
	testb $3, 144(%rsp)             # cs: returning to user mode?
	jz 1f
	cli
	movq %rsp, %rdi
	call return_to_user             # signals, rescheduling (interrupts off)
1:	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rax
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	addq $16, %rsp
	testb $3, 8(%rsp)               # cs: to user mode? give it its %gs base back
	jz 2f
	swapgs
2:	iretq

# NMI, double fault, machine check: whichever %gs base is loaded, make it the
# kernel's (these never return to kernel mode, they panic)
isr_paranoid:
	pushq %rax
	pushq %rcx
	pushq %rdx
	movl $0xC0000101, %ecx          # MSR_GS_BASE
	rdmsr
	testl %edx, %edx                # the kernel's base has bit 63 set
	popq %rdx
	popq %rcx
	popq %rax
	js isr_save
	swapgs
	jmp isr_save

# system call entry (MSR_LSTAR).  rcx = user rip, r11 = user rflags,
# interrupts are off (SFMASK).  The kernel stack and a scratch slot are the
# CPU's own (struct cpu_info, offsets 8 and 16).
	.globl syscall_entry
syscall_entry:
	swapgs
	movq %rsp, %gs:16
	movq %gs:8, %rsp
	pushq $0x2b                     # ss
	pushq %gs:16                    # rsp
	pushq %r11                      # rflags
	pushq $0x33                     # cs
	pushq %rcx                      # rip
	pushq %rax                      # err = orig_rax
	pushq $0x100                    # vector: system call
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	sti
	cld
	movq %rsp, %rdi
	call syscall_dispatch
	jmp trap_return

# void switch_stack(uint64_t *save_sp, uint64_t new_sp)
	.globl switch_stack
switch_stack:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq %rsp, (%rdi)
	movq %rsi, %rsp
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	ret

# first return of a new task: a new user task goes out through trap_return
# (its frame is right above), a kernel thread calls r12(r13).
	.globl ret_from_fork, kthread_start
ret_from_fork:
	call schedule_tail
	jmp trap_return
kthread_start:
	call schedule_tail
	sti
	movq %r13, %rdi
	call *%r12
	movq %rax, %rdi
	call kthread_exit

# long user_copy(void *dst, const void *src, size_t n): 0, or -1 on a fault
# long user_strnlen(const char *s, size_t max): length, max if no NUL within
#   max bytes, -1 on a fault
# long user_set(void *dst, int c, size_t n): 0 or -1
# A fault inside [user_access_begin, user_access_end) that the page fault
# handler cannot resolve resumes at user_access_fault.
	.globl user_copy, user_strnlen, user_set, user_access_begin, user_access_end, user_access_fault
user_access_begin:
user_copy:
	movq %rdx, %rcx
	rep movsb
	xorl %eax, %eax
	ret
user_set:
	movl %esi, %eax
	movq %rdx, %rcx
	rep stosb
	xorl %eax, %eax
	ret
user_strnlen:
	xorl %eax, %eax
1:	cmpq %rsi, %rax
	je 2f
	cmpb $0, (%rdi,%rax)
	je 2f
	incq %rax
	jmp 1b
2:	ret
user_access_end:
user_access_fault:
	movq $-1, %rax
	ret

# void enter_user(struct regs *r): leave for user mode with frame r
	.globl enter_user
enter_user:
	movq %rdi, %rsp
	jmp trap_return
