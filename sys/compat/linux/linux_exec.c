/* linux_exec.c - the Linux emulation: a program that is not branded as
   V86BSD's runs by Linux's system call numbers (kern_exec.c chooses). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <compat/linux/linux_syscall.h>

extern const struct sysent linux_sysent[];

const struct emul emul_linux = {
    "linux", linux_sysent, LINUX_SYS_MAXSYSCALL, LINUX_SYS_rt_sigreturn, "Linux", "6.9.0",
};
