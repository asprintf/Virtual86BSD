#include <stdlib.h>
#include <stddef.h>
#include "libc.h"

void __stdio_exit_weak(void);

extern void (*const __fini_array_start[])(void) __attribute__((weak));
extern void (*const __fini_array_end[])(void) __attribute__((weak));
void _fini(void);

void __libc_exit_fini(void)
{
	if (__libc.dl_fini) {   /* dynamically linked: every module's */
		__libc.dl_fini();
		return;
	}
	size_t n = (size_t)(__fini_array_end - __fini_array_start);
	while (n) __fini_array_start[--n]();
	_fini();
}

_Noreturn void exit(int status)
{
	__funcs_on_exit();
	__libc_exit_fini();
	__stdio_exit_weak();
	_Exit(status);
}
