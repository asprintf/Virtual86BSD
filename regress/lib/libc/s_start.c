/* Start-up, TLS, exit and error-string behaviour. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include "out.h"

_Thread_local int tls_init = 1234;
_Thread_local long tls_zero[64];
_Alignas(64) _Thread_local char tls_aligned[3] = "ab";
static int ctor_ran;
#ifdef __GNUC__
__attribute__((constructor)) static void ctor(void) { ctor_ran = 1; }
#else
/* c17 has no constructor attribute: place a pointer in .init_array by hand */
void s_start_ctor(void);
void s_start_ctor(void) { ctor_ran = 1; }
__asm__(".section .init_array,\"aw\"\n.align 8\n.quad s_start_ctor\n.text");
#endif

static void h1(void) { os("h1\n"); oflush(); }
static void h2(void) { os("h2\n"); oflush(); atexit(h1); }  /* registered during exit */
static void h3(void) { os("h3\n"); oflush(); }

extern char **environ;
int main(int argc, char **argv, char **envp)
{
	char buf[64];
	int i;
	CHECK(ctor_ran);
	CHECK(argc >= 1 && argv[0] && argv[argc] == 0);
	CHECK(envp == environ);
	CHECK(tls_init == 1234);
	for (i = 0; i < 64; i++) CHECK(tls_zero[i] == 0);
	CHECK(((unsigned long)tls_aligned & 63) == 0 && !strcmp(tls_aligned, "ab"));
	errno = 0; CHECK(errno == 0); errno = EDOM; CHECK(errno == EDOM);
	CHECK(close(-1) == -1 && errno == EBADF);

	CHECK(!strcmp(strerror(0), "Success"));
	CHECK(!strcmp(strerror(ENOENT), "No such file or directory"));
	CHECK(!strcmp(strerror(-5), "Unknown error -5"));
	CHECK(!strcmp(strerror(100000), "Unknown error 100000"));
	CHECK(strerror_r(ENOENT, buf, sizeof buf) == 0 && !strcmp(buf, "No such file or directory"));
	CHECK(strerror_r(ENOENT, buf, 5) == ERANGE && !strcmp(buf, "No s"));
	CHECK(strerror_r(9999, buf, sizeof buf) == EINVAL && !strcmp(buf, "Unknown error 9999"));
	for (i = 1; i < 134; i++) CHECK(strerror(i) != 0 && strlen(strerror(i)) > 3);
	CHECK(!strcmp(strsignal(11), "Segmentation fault"));
	CHECK(!strcmp(strsignal(40), "Real-time signal 5"));
	CHECK(!strcmp(strsignal(0), "Unknown signal 0"));
	CHECK(getenv("PATH") != 0 || 1);
	CHECK(getenv("") == 0 && getenv("A=B") == 0);
	CHECK(EXIT_SUCCESS == 0 && PATH_MAX == 4096);
	assert(1 + 1 == 2);
	{ char *e; errno = 0; CHECK(strtol("12", &e, 1) == 0 && errno == EINVAL);
	  errno = 0; CHECK(strtoul("12", &e, 37) == 0 && errno == EINVAL); }

	atexit(h1); atexit(h2); atexit(h3);
	for (i = 0; i < 100; i++) CHECK(atexit(h3) == 0);  /* beyond the first block */
	os(failures ? "start FAILED\n" : "start ok\n");
	oflush();
	return failures ? 1 : 0;
}
