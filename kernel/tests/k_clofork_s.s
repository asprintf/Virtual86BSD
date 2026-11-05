# long rawgetfd(int fd): fcntl(fd, F_GETFD) straight to the kernel,
# bypassing the C library's emulation of FD_CLOFORK
	.text
	.globl rawgetfd
rawgetfd:
	movslq %edi, %rdi
	movl $1, %esi          # F_GETFD
	movl $72, %eax         # SYS_fcntl
	syscall
	ret
