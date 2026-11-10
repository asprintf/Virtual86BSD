/* libdlb.so: needs libdla.so (found through DT_RUNPATH $ORIGIN); a
 * function found with RTLD_NEXT. */
#define _GNU_SOURCE 1
#include <dlfcn.h>
#include <string.h>

int dla_add(int);

int dlb_call(int x) { return dla_add(x) * 2; }

/* strlen of the next object in the global scope (libc.so) */
size_t dlb_next_strlen(const char *s)
{
	size_t (*f)(const char *) = (size_t (*)(const char *))dlsym(RTLD_NEXT, "strlen");
	return f ? f(s) + 1000 : 0;
}
