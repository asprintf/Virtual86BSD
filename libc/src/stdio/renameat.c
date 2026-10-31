#include "stdio_impl.h"
#include "syscall.h"

int renameat(int fa, const char *a, int fb, const char *b)
{
	return (int)__sys4(SYS_renameat, fa, a, fb, b);
}

char *ctermid(char *s)
{
	static char buf[L_ctermid];
	char *p = s ? s : buf;
	const char *t = "/dev/tty";
	int i;
	for (i = 0; (p[i] = t[i]); i++);
	return p;
}
