#include <sys/wait.h>
#include <errno.h>
#include "syscall.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
	return (pid_t)__sys_cp4(SYS_wait4, pid, status, options, 0);
}

pid_t wait(int *status)
{
	return waitpid(-1, status, 0);
}

int waitid(idtype_t type, id_t id, siginfo_t *info, int options)
{
	return (int)__sys_cp5(SYS_waitid, type, id, info, options, 0);
}
