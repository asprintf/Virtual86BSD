#ifndef _DLFCN_H
#define _DLFCN_H
#include <bits/features.h>

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_NOLOAD 4
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL  0

typedef struct {
	const char *dli_fname;
	void *dli_fbase;
	const char *dli_sname;
	void *dli_saddr;
} Dl_info_t;

#if __EXT_VISIBLE
typedef Dl_info_t Dl_info;
#define RTLD_NEXT    ((void *)-1)
#define RTLD_DEFAULT ((void *)0)
#endif

__BEGIN_DECLS
int dladdr(const void *__RESTRICT, Dl_info_t *__RESTRICT);
int dlclose(void *);
char *dlerror(void);
void *dlopen(const char *, int);
void *dlsym(void *__RESTRICT, const char *__RESTRICT);
__END_DECLS

#endif
