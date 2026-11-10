# boot.s - entry from a Multiboot (version 1) loader such as QEMU's -kernel
# or GRUB.  The loader leaves us in 32-bit protected mode, paging off,
# %eax = 0x2BADB002, %ebx = physical address of the multiboot information.
#
# We build boot page tables that map the first GiB of physical memory three
# times: at 0 (identity, only for the switch), at DIRECT_MAP
# (0xffff888000000000) and at KERNEL_BASE (0xffffffff80000000), turn on long
# mode and jump to kmain() in the higher half.
#
# The image is linked at KERNEL_BASE + 1 MiB and loaded at 1 MiB; 32-bit code
# therefore uses "symbol - KERNEL_BASE" for physical addresses.  tools/mkimage
# fills in the load addresses of the multiboot header (address-kludge form,
# so the loader needs no ELF support).  The header is alone in section
# .boot, which both binutils' ld (kernel.ld) and c17's link editor place
# at the start of the image, within the 8 KiB a loader searches.

	.set KERNEL_BASE, 0xffffffff80000000

	.section .boot, "a"
	.globl multiboot_header
	.balign 8
multiboot_header:
	.long 0x1BADB002                # magic
	.long 0x00010003                # flags: align, memory map, address fields
	.long -(0x1BADB002 + 0x00010003)
	.long 0                         # header_addr     (filled in by mkimage)
	.long 0                         # load_addr
	.long 0                         # load_end_addr
	.long 0                         # bss_end_addr
	.long 0                         # entry_addr

	# the 32-bit entry code (anywhere in .text: the header holds its
	# address)
	.text
	.code32
	.globl _start32
_start32:
	cli
	movl %eax, %edi                 # multiboot magic -> kmain's 1st argument
	movl %ebx, %esi                 # info -> 2nd argument

	# boot_pd: 512 2-MiB pages covering the first GiB
	movl $(boot_pd - KERNEL_BASE), %ebx
	xorl %ecx, %ecx
1:	movl %ecx, %eax
	shll $21, %eax
	orl $0x83, %eax                 # present, writable, 2 MiB page
	movl %eax, (%ebx,%ecx,8)
	movl $0, 4(%ebx,%ecx,8)
	incl %ecx
	cmpl $512, %ecx
	jne 1b

	# boot_pdpt_low[0] -> boot_pd; boot_pdpt_high[510] -> boot_pd
	movl $(boot_pd - KERNEL_BASE + 3), %eax
	movl %eax, (boot_pdpt_low - KERNEL_BASE)
	movl %eax, (boot_pdpt_high - KERNEL_BASE + 510 * 8)

	# pml4[0] and pml4[273] -> boot_pdpt_low; pml4[511] -> boot_pdpt_high
	movl $(boot_pdpt_low - KERNEL_BASE + 3), %eax
	movl %eax, (boot_pml4 - KERNEL_BASE)
	movl %eax, (boot_pml4 - KERNEL_BASE + 273 * 8)
	movl $(boot_pdpt_high - KERNEL_BASE + 3), %eax
	movl %eax, (boot_pml4 - KERNEL_BASE + 511 * 8)

	movl $(boot_pml4 - KERNEL_BASE), %eax
	movl %eax, %cr3
	movl %cr4, %eax
	orl $0x20, %eax                 # PAE
	movl %eax, %cr4
	movl $0xC0000080, %ecx          # EFER
	rdmsr
	orl $0x900, %eax                # LME | NXE
	wrmsr
	movl %cr0, %eax
	orl $0x80010001, %eax           # PG | WP | PE
	movl %eax, %cr0

	lgdt (boot_gdt_ptr - KERNEL_BASE)
	ljmp $0x10, $(start64_low - KERNEL_BASE)

	.code64
start64_low:
	movabsq $start64, %rax
	jmp *%rax

	.balign 8
boot_gdt:
	.quad 0
	.quad 0x00cf9a000000ffff        # 0x08: 32-bit code
	.quad 0x00af9a000000ffff        # 0x10: 64-bit kernel code
	.quad 0x00cf92000000ffff        # 0x18: kernel data
boot_gdt_end:
boot_gdt_ptr:
	.word boot_gdt_end - boot_gdt - 1
	.long boot_gdt - KERNEL_BASE

	.text
start64:
	movw $0x18, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	xorw %ax, %ax
	movw %ax, %fs
	movw %ax, %gs
	leaq boot_stack_top(%rip), %rsp
	xorl %ebp, %ebp
	call kmain
2:	hlt
	jmp 2b

	.bss
	.balign 4096
	.globl boot_pml4
boot_pml4:	.zero 4096
boot_pdpt_low:	.zero 4096
boot_pdpt_high:	.zero 4096
boot_pd:	.zero 4096
	.globl boot_stack, boot_stack_top
boot_stack:	.zero 16384
boot_stack_top:
