#include <stdlib.h>
#include "syscall.h"

_Noreturn void _Exit(int status)
{
	__syscall1(SYS_exit_group, status);
	for (;;) __syscall1(SYS_exit, status);
}
