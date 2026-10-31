#include <errno.h>
#include "stdio_impl.h"
#include "syscall.h"

int remove(const char *path)
{
	long r = __syscall3(SYS_unlinkat, -100, path, 0);
	if (r == -EISDIR) r = __syscall3(SYS_unlinkat, -100, path, 0x200 /* AT_REMOVEDIR */);
	return (int)__syscall_ret((unsigned long)r);
}

int rename(const char *a, const char *b)
{
	return (int)__sys4(SYS_renameat, -100, a, -100, b);
}
