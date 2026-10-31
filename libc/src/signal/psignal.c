/* psignal(), psiginfo(): messages on standard error.
 * TODO(stdio): write through stderr once <stdio.h> exists, as POSIX
 * requires (orientation, error indicator). */
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "syscall.h"

static void out(const char *a, const char *b)
{
	struct { const void *p; size_t n; } v[4];
	int n = 0;
	if (a && *a) { v[n].p = a; v[n++].n = strlen(a); v[n].p = ": "; v[n++].n = 2; }
	v[n].p = b; v[n++].n = strlen(b);
	v[n].p = "\n"; v[n++].n = 1;
	__syscall3(SYS_writev, 2, v, n);
}

void psignal(int sig, const char *msg)
{
	int e = errno;
	out(msg, strsignal(sig));
	errno = e;
}

void psiginfo(const siginfo_t *si, const char *msg)
{
	psignal(si->si_signo, msg);
}
