/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
