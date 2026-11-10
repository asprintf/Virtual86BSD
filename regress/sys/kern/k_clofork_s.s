# long rawgetfd_nr(int fd, long nr): fcntl(fd, F_GETFD) straight to the kernel
# (nr is its system call number: the ABI's), bypassing the C library's
# emulation of FD_CLOFORK
	.text
	.globl rawgetfd_nr
rawgetfd_nr:
	movslq %edi, %rdi
	movq %rsi, %rax        # SYS_fcntl
	movl $1, %esi          # F_GETFD
	syscall
	ret
