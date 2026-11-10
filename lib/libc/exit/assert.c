#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"

static void put(const char *s) { __syscall3(SYS_write, 2, s, strlen(s)); }

/* TODO: route through stderr once <stdio.h> exists. */
_Noreturn void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
	char num[24], *p = num + sizeof num;
	unsigned u = (unsigned)line;
	*--p = 0;
	do *--p = (char)('0' + u % 10); while (u /= 10);
	put("Assertion failed: "); put(expr);
	put(" ("); put(file); put(": "); put(func); put(": "); put(p); put(")\n");
	abort();
}
