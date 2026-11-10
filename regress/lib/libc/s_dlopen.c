/* s_dlopen.c - <dlfcn.h>.  Linked dynamically (run.sh with
 * LIBC_LINK=dynamic builds the objects of tests/dl and sets TEST_DL_DIR):
 * dlopen with dependencies, RTLD_LOCAL and RTLD_GLOBAL, constructors and
 * destructors, dynamic TLS in several threads, RTLD_DEFAULT, RTLD_NEXT,
 * dladdr, errors (missing object, undefined symbol, initial-exec TLS).
 * Linked statically: dlopen of a file fails; the program's own symbols. */
#define _GNU_SOURCE 1
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

extern char **environ;

int s_dlopen_marker(void) { return 42; }

static void *(*tls_ptr)(void);
static char *(*tls_big)(void);

static void *thread(void *arg)
{
	int *p = tls_ptr();
	char *b = tls_big();
	(void)arg;
	/* a new thread: a fresh block from the image */
	if (*p != 5 || strcmp(b, "image")) return (void *)1;
	*p = 9;
	if (*(int *)tls_ptr() != 9) return (void *)2;
	return 0;
}

static char path[4096];

static const char *lib(const char *dir, const char *name)
{
	snprintf(path, sizeof path, "%s/%s", dir, name);
	return path;
}

int main(void)
{
	const char *dir = getenv("TEST_DL_DIR");
	void *self, *a, *a2, *b, *x;
	const char *e;
	Dl_info info;

	self = dlopen(0, RTLD_NOW);
	CHECK(self != 0);
	CHECK(dlsym(self, "s_dlopen_marker") == (void *)s_dlopen_marker);
	CHECK(dladdr((void *)s_dlopen_marker, &info) && info.dli_sname
	      && !strcmp(info.dli_sname, "s_dlopen_marker"));
	CHECK(dlsym(self, "no_such_symbol_xyz") == 0 && dlerror() != 0);
	CHECK(dlerror() == 0);
	CHECK(dlopen(0, 0) == 0 && dlerror() != 0);   /* neither LAZY nor NOW */

	if (!dir) {
		/* a static program */
		CHECK(dlopen("libanything.so", RTLD_NOW) == 0 && dlerror() != 0);
		os("static: ok\n");
		oflush();
		return failures != 0;
	}
	CHECK(dlsym(self, "printf") == (void *)printf);
	CHECK(dlsym(RTLD_DEFAULT, "strlen") == (void *)strlen);

	/* errors */
	CHECK(dlopen(lib(dir, "libnonexistent.so"), RTLD_NOW) == 0);
	CHECK((e = dlerror()) && strstr(e, "libnonexistent.so"));
	CHECK(dlopen(lib(dir, "libdlundef.so"), RTLD_NOW) == 0);
	CHECK((e = dlerror()) && strstr(e, "no_such_function_anywhere"));
	CHECK(dlopen(lib(dir, "libdlie.so"), RTLD_NOW) == 0);
	CHECK((e = dlerror()) && strstr(e, "TLS"));

	/* RTLD_LOCAL */
	a = dlopen(lib(dir, "libdla.so"), RTLD_NOW | RTLD_LOCAL);
	CHECK(a != 0);
	if (!a) { os(dlerror()); onl(); oflush(); return 1; }
	CHECK(*(int *)dlsym(a, "dla_ctor_ran") == 1);
	CHECK(((int (*)(int))dlsym(a, "dla_add"))(2) == 43);
	CHECK(dlsym(RTLD_DEFAULT, "dla_add") == 0 && dlerror());
	a2 = dlopen(lib(dir, "libdla.so"), RTLD_LAZY);
	CHECK(a2 == a);
	CHECK(*(int *)dlsym(a, "dla_ctor_ran") == 1);       /* once */
	CHECK(dlopen("libdla.so", RTLD_NOW | RTLD_NOLOAD) == a);

	/* the same C library: malloc, stdio, errno, environ */
	{
		char *s = ((char *(*)(const char *))dlsym(a, "dla_strdup"))("dup");
		CHECK(s && !strcmp(s, "dup"));
		free(s);
		CHECK(((FILE *(*)(void))dlsym(a, "dla_stdout"))() == stdout);
		errno = 0;
		((void (*)(int))dlsym(a, "dla_set_errno"))(ERANGE);
		CHECK(errno == ERANGE);
		CHECK(((char **(*)(void))dlsym(a, "dla_environ"))() == environ);
	}

	/* dynamic TLS: this thread, another, the address from dlsym */
	tls_ptr = (void *(*)(void))dlsym(a, "dla_tls_ptr");
	tls_big = (char *(*)(void))dlsym(a, "dla_tls_big_ptr");
	CHECK(tls_ptr && tls_big);
	if (tls_ptr && tls_big) {
		pthread_t t[4];
		void *r;
		int i;
		CHECK(*(int *)tls_ptr() == 5 && !strcmp(tls_big(), "image"));
		*(int *)tls_ptr() = 7;
		CHECK(dlsym(a, "dla_tls") == tls_ptr());
		for (i = 0; i < 4; i++) CHECK(pthread_create(&t[i], 0, thread, 0) == 0);
		for (i = 0; i < 4; i++) { CHECK(pthread_join(t[i], &r) == 0); CHECK(r == 0); }
		CHECK(*(int *)tls_ptr() == 7);
	}

	/* dladdr in a shared object */
	CHECK(dladdr(dlsym(a, "dla_add"), &info));
	CHECK(info.dli_fname && strstr(info.dli_fname, "libdla.so"));
	CHECK(info.dli_sname && !strcmp(info.dli_sname, "dla_add"));
	CHECK(dladdr((char *)dlsym(a, "dla_add") + 1, &info) && info.dli_saddr == dlsym(a, "dla_add"));
	/* (a function the program does not use: no canonical PLT entry) */
	CHECK(dladdr(dlsym(RTLD_DEFAULT, "wcsxfrm"), &info) && info.dli_fname && strstr(info.dli_fname, "libc.so"));
	CHECK(info.dli_sname && !strcmp(info.dli_sname, "wcsxfrm"));

	/* a dependency through $ORIGIN, RTLD_GLOBAL, RTLD_NEXT */
	b = dlopen(lib(dir, "libdlb.so"), RTLD_NOW | RTLD_GLOBAL);
	CHECK(b != 0);
	if (b) {
		CHECK(((int (*)(int))dlsym(b, "dlb_call"))(1) == 84);
		CHECK(dlsym(b, "dla_add") == dlsym(a, "dla_add"));   /* its dependency */
		CHECK(dlsym(RTLD_DEFAULT, "dlb_call") != 0);
		CHECK(dlsym(RTLD_DEFAULT, "dla_add") == dlsym(a, "dla_add"));   /* global now */
		CHECK(((size_t (*)(const char *))dlsym(b, "dlb_next_strlen"))("abc") == 1003);
	}

	CHECK(dlclose(a2) == 0 && dlclose(a) == 0);
	x = (void *)&path;
	CHECK(dlclose(x) == -1 && dlerror());
	os("dynamic: ok\n");
	oflush();
	return failures != 0;
}
