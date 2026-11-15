/* the linker script of stage 2 (stage 1 loads it to 0:8000) */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
	. = 0x8000;
	.text16 : { *(.text16) *(.data16) }
	.text : { *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	.data : { *(.data .data.*) }
	. = ALIGN(4);
	_edata = .;
	.bss : {
		__bss_start = .;
		*(.bss .bss.*) *(COMMON)
		. = ALIGN(4);
		__bss_end = .;
	}
	_end = .;
	/DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) *(.gnu*) }
}
ASSERT(SIZEOF(.text16) + 0x8000 <= 0x10000, "the real-mode part is beyond 64 KiB")
ASSERT(_end <= 0x60000, "the loader is too big")
