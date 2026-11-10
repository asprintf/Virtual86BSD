/* <dlfcn.h> for statically linked programs (libc.a; libc.so has the
 * dynamic linker's).  There is nothing to load: dlopen of a file fails.
 * dlopen(NULL) gives a handle for the program itself, whose symbols
 * dlsym and dladdr find in the ELF symbol table of /proc/self/exe (none
 * if the program was stripped).  dlerror is per thread. */
#include <dlfcn.h>
#include "pthread_impl.h"

void *__selfsym_lookup(const char *);
int __selfsym_present(void);
int __selfsym_addr(const void *, Dl_info_t *);

static char self;

static void *seterr(const char *msg)
{
	__pthread_self()->dlerror = msg;
	return 0;
}

void *dlopen(const char *file, int mode)
{
	int how = mode & (RTLD_LAZY | RTLD_NOW);
	if (how != RTLD_LAZY && how != RTLD_NOW)
		return seterr("dlopen: invalid mode");
	if (file)
		return seterr("dlopen: dynamic loading is not supported (statically linked program)");
	return &self;
}

int dlclose(void *h)
{
	if (h != &self) {
		seterr("dlclose: invalid handle");
		return -1;
	}
	return 0;
}

char *dlerror(void)
{
	struct __pthread *t = __pthread_self();
	const char *m = t->dlerror;
	t->dlerror = 0;
	return (char *)m;
}

void *dlsym(void *__RESTRICT h, const char *__RESTRICT name)
{
	void *p;
	if (h != &self)
		return seterr("dlsym: invalid handle");
	if (!__selfsym_present())
		return seterr("dlsym: the program has no symbol table");
	if (!(p = __selfsym_lookup(name)))
		return seterr("dlsym: symbol not found");
	return p;
}

int dladdr(const void *__RESTRICT addr, Dl_info_t *__RESTRICT info)
{
	if (!__selfsym_addr(addr, info)) {
		seterr("dladdr: address not in a loaded object");
		return 0;
	}
	return 1;
}
