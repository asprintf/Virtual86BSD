/* kernel.ld - the kernel at KERNEL_BASE + 1 MiB, loaded at 1 MiB */
ENTRY(_start32)
SECTIONS
{
  . = 0xffffffff80100000;
  .text : AT(0x100000) { *(.boot) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .bss : { *(.bss .bss.*) *(COMMON) }
  . = ALIGN(4096);
  _kernel_end = .;
  /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame) }
}
