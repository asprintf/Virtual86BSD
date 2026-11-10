# The brand of a V86BSD program (crt1.o, Scrt1.o, rcrt1.o of the native C
# library): an ELF note, name "V86BSD", type 1, a descriptor of one word (0).
# The linker keeps it and writes a PT_NOTE for it, and the kernel's exec
# runs a program that has it by the native system call numbers (a program
# without it is Linux's).
	.section .note.v86bsd.ident,"a",@note
	.balign	4
	.long	7		# namesz ("V86BSD" and its NUL)
	.long	4		# descsz
	.long	1		# type: the brand
	.asciz	"V86BSD"
	.balign	4
	.long	0
