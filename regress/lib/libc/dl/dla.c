/* libdla.so: for s_dlopen.c.  Constructor and destructor, a global and a
 * thread-local variable (general-dynamic TLS), and calls into libc.so. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int dla_ctor_ran, dla_counter = 40;
__thread int dla_tls = 5;
__thread char dla_tls_big[100] = "image";

__attribute__((constructor)) static void ctor(void) { dla_ctor_ran++; dla_counter++; }
__attribute__((destructor)) static void dtor(void)
{
	/* (the test's output ends with this line) */
	static const char m[] = "libdla: destructor\n";
	write(1, m, sizeof m - 1);
}

int dla_add(int x) { return x + dla_counter; }
int *dla_tls_ptr(void) { return &dla_tls; }
char *dla_tls_big_ptr(void) { return dla_tls_big; }
char *dla_strdup(const char *s) { return strdup(s); }
FILE *dla_stdout(void) { return stdout; }
void dla_set_errno(int e) { errno = e; }
char **dla_environ(void) { extern char **environ; return environ; }
