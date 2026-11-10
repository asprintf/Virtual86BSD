#include <unistd.h>
#include "syscall.h"

pid_t getpid(void) { return (pid_t)__syscall0(SYS_getpid); }
pid_t getppid(void) { return (pid_t)__syscall0(SYS_getppid); }
pid_t getpgrp(void) { return (pid_t)__syscall1(SYS_getpgid, 0); }
pid_t getpgid(pid_t p) { return (pid_t)__sys1(SYS_getpgid, p); }
pid_t getsid(pid_t p) { return (pid_t)__sys1(SYS_getsid, p); }
int setpgid(pid_t p, pid_t g) { return (int)__sys2(SYS_setpgid, p, g); }
pid_t setsid(void) { return (pid_t)__sys0(SYS_setsid); }
