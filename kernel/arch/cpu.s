# cpu.s - small functions for instructions C cannot express (the kernel has
# no inline assembly, so that the project's own c17 can compile it).

	.text
	.globl outb, outw, outl, inb, inw, inl, insw, outsw
outb:	movl %esi, %eax
	movl %edi, %edx
	outb %al, %dx
	ret
outw:	movl %esi, %eax
	movl %edi, %edx
	outw %ax, %dx
	ret
outl:	movl %esi, %eax
	movl %edi, %edx
	outl %eax, %dx
	ret
inb:	movl %edi, %edx
	xorl %eax, %eax
	inb %dx, %al
	ret
inw:	movl %edi, %edx
	xorl %eax, %eax
	inw %dx, %ax
	ret
inl:	movl %edi, %edx
	inl %dx, %eax
	ret
# insw(port, buf, count), outsw(port, buf, count): count 16-bit words
insw:	movq %rdx, %rcx
	movl %edi, %edx
	movq %rsi, %rdi
	rep insw
	ret
outsw:	movq %rdx, %rcx
	movl %edi, %edx
	rep outsw
	ret

	.globl read_cr0, read_cr2, read_cr3, read_cr4, write_cr0, write_cr3, write_cr4
read_cr0:	movq %cr0, %rax
	ret
read_cr2:	movq %cr2, %rax
	ret
read_cr3:	movq %cr3, %rax
	ret
read_cr4:	movq %cr4, %rax
	ret
write_cr0:	movq %rdi, %cr0
	ret
write_cr3:	movq %rdi, %cr3
	ret
write_cr4:	movq %rdi, %cr4
	ret

	.globl invlpg
invlpg:	invlpg (%rdi)
	ret

# uint64_t rdmsr(uint32_t msr); void wrmsr(uint32_t msr, uint64_t v)
	.globl rdmsr, wrmsr
rdmsr:	movl %edi, %ecx
	rdmsr
	shlq $32, %rdx
	orq %rdx, %rax
	ret
wrmsr:	movl %edi, %ecx
	movl %esi, %eax
	movq %rsi, %rdx
	shrq $32, %rdx
	wrmsr
	ret

	.globl rdtsc64
rdtsc64:	rdtsc
	shlq $32, %rdx
	orq %rdx, %rax
	ret

# lgdt_reload(ptr): load the GDT and reload every segment register
	.globl lgdt_reload, lidt, ltr
lgdt_reload:
	lgdt (%rdi)
	movw $0x18, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	xorw %ax, %ax
	movw %ax, %fs
	movw %ax, %gs
	popq %rax
	pushq $0x10
	pushq %rax
	lretq
lidt:	lidt (%rdi)
	ret
ltr:	movw %di, %ax
	ltr %ax
	ret

	.globl cli, sti, hlt, irq_save, irq_restore, sti_hlt, read_rflags
cli:	cli
	ret
sti:	sti
	ret
hlt:	hlt
	ret
# wait for an interrupt with interrupts enabled, return with them disabled
sti_hlt:
	sti
	hlt
	cli
	ret
read_rflags:
	pushfq
	popq %rax
	ret
# unsigned long irq_save(void): disable interrupts, return the old flags
irq_save:
	pushfq
	popq %rax
	cli
	ret
irq_restore:
	testl $0x200, %edi
	jz 1f
	sti
1:	ret

	.globl fxsave, fxrstor, fninit_sse
fxsave:	fxsave (%rdi)
	ret
fxrstor:	fxrstor (%rdi)
	ret
# a clean x87/SSE state as Linux gives a new process
fninit_sse:
	fninit
	subq $8, %rsp
	movl $0x1f80, (%rsp)
	ldmxcsr (%rsp)
	addq $8, %rsp
	ret

	.globl cpuid4
# cpuid4(leaf, subleaf, uint32_t out[4])
cpuid4:	pushq %rbx
	movq %rdx, %r8
	movl %edi, %eax
	movl %esi, %ecx
	cpuid
	movl %eax, (%r8)
	movl %ebx, 4(%r8)
	movl %ecx, 8(%r8)
	movl %edx, 12(%r8)
	popq %rbx
	ret

# mb(): a full memory barrier (and, being a call, a barrier to the
# compiler); cpu_pause(): the spin-wait hint
	.globl mb, cpu_pause
mb:	mfence
	ret
cpu_pause:	pause
	ret
