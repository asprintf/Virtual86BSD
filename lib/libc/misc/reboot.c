/* reboot(2): V86BSD's (<sys/reboot.h>); on the Linux ABI the same howto is
   turned into Linux's three arguments. */
#include <abi.h>
#include <sys/reboot.h>
#include "syscall.h"

int reboot(int howto)
{
#ifdef __V86BSD_NATIVE_ABI
	return __sys1(SYS_reboot, howto);
#else
	long cmd = 0x01234567;	/* LINUX_REBOOT_CMD_RESTART */
	if (howto & RB_POWERDOWN) cmd = 0x4321fedc;	/* POWER_OFF */
	else if (howto & RB_HALT) cmd = 0xcdef0123;	/* HALT */
	return __sys3(SYS_reboot, 0xfee1dead, 672274793, cmd);
#endif
}
