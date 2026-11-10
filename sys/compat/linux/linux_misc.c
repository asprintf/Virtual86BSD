/* linux_misc.c - the system calls that are Linux's own: what a Linux
   program sees where V86BSD's answer would differ. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <uvm/uvm_extern.h>
#include <compat/linux/linux_syscallargs.h>

extern char uts_nodename[65], uts_domain[65];

/* uname: Linux, release 6.9.0, x86_64 (the Linux kernel the ABI is that of) */
long linux_sys_uname(long ubuf) {
  char u[6][65];
  memset(u, 0, sizeof(u));
  strcpy(u[0], emul_linux.e_ostype);
  strcpy(u[1], uts_nodename);
  strcpy(u[2], emul_linux.e_osrelease);
  strcpy(u[3], "#1 V86BSD");
  strcpy(u[4], "x86_64");
  strcpy(u[5], uts_domain);
  return copy_to_user((void *)ubuf, u, sizeof(u)) ? -EFAULT : 0;
}

long linux_sys_personality(long v) {
  (void)v;
  return 0;
}

/* reboot(2) of Linux: the magic numbers and LINUX_REBOOT_CMD_* */
#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_CMD_RESTART 0x01234567
#define LINUX_REBOOT_CMD_HALT 0xcdef0123
#define LINUX_REBOOT_CMD_CAD_ON 0x89abcdef
#define LINUX_REBOOT_CMD_CAD_OFF 0
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedc

long linux_sys_reboot(long m1, long m2, long cmd) {
  (void)m2;
  if (!capable())
    return -EPERM;
  if ((uint32_t)m1 != LINUX_REBOOT_MAGIC1)
    return -EINVAL;
  switch ((uint32_t)cmd) {
  case LINUX_REBOOT_CMD_CAD_ON:
  case LINUX_REBOOT_CMD_CAD_OFF:
    return 0;
  case LINUX_REBOOT_CMD_RESTART:
    return sys_reboot(RB_AUTOBOOT);
  case LINUX_REBOOT_CMD_POWER_OFF:
    return sys_reboot(RB_POWERDOWN | RB_HALT);
  case LINUX_REBOOT_CMD_HALT:
    return sys_reboot(RB_HALT);
  }
  return -EINVAL;
}
