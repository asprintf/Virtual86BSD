# poweroff.s - reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
# LINUX_REBOOT_CMD_POWER_OFF): Shall's C library has no reboot().
	.text
	.globl linux_poweroff
linux_poweroff:
	movl $169, %eax
	movl $0xfee1dead, %edi
	movl $672274793, %esi
	movl $0x4321fedc, %edx
	syscall
	ret
