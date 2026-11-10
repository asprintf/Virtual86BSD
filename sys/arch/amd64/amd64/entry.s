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

	STUB_NOERR 0
	STUB_NOERR 1
	STUB_NOERR 2
	STUB_NOERR 3
	STUB_NOERR 4
	STUB_NOERR 5
	STUB_NOERR 6
	STUB_NOERR 7
	STUB_ERR 8
	STUB_NOERR 9
	STUB_ERR 10
	STUB_ERR 11
	STUB_ERR 12
	STUB_ERR 13
	STUB_ERR 14
	STUB_NOERR 15
	STUB_NOERR 16
	STUB_ERR 17
	STUB_NOERR 18
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

	.data
	.globl isr_table
isr_table:
	.quad isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
	.quad isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
	.quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
	.quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	.quad isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
	.quad isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
	.text

isr_common:
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
	iretq

# system call entry (MSR_LSTAR).  rcx = user rip, r11 = user rflags,
# interrupts are off (SFMASK).  Single processor: the kernel stack and a
# scratch slot are plain globals.
	.globl syscall_entry
syscall_entry:
	movq %rsp, syscall_user_rsp(%rip)
	movq kstack_top(%rip), %rsp
	pushq $0x2b                     # ss
	pushq syscall_user_rsp(%rip)    # rsp
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

	.data
	.globl kstack_top
kstack_top:	.quad 0
syscall_user_rsp:	.quad 0
	.text

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
