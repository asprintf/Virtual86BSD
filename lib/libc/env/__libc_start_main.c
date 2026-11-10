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

/* Process start-up: called from _start (arch/x86_64/crt1.s). */
#include <stddef.h>
#include "libc.h"
#include "elf_min.h"
#include "pthread_impl.h"
#include "syscall.h"

struct __libc __libc;
char *__progname, *__progname_full;    /* argv[0], and its basename */


extern void (*const __preinit_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*const __preinit_array_end[])(int, char **, char **) __attribute__((weak));
extern void (*const __init_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*const __init_array_end[])(int, char **, char **) __attribute__((weak));
void _init(void);
void exit(int);

unsigned long __getauxval(unsigned long type)
{
	unsigned long *a = __libc.auxv;
	for (; a && a[0]; a += 2)
		if (a[0] == type) return a[1];
	return 0;
}

void __libc_start_main(long *sp, int (*main)(int, char **, char **))
{
	int argc = (int)sp[0];
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1;
	unsigned long *auxv;
	char **e;
	size_t n, i;

	__environ = envp;
	if (argc > 0 && argv[0]) {
		char *p;
		__progname = __progname_full = argv[0];
		for (p = argv[0]; *p; p++)
			if (*p == '/')
				__progname = p + 1;
	}
	for (e = envp; *e; e++);
	auxv = (unsigned long *)(e + 1);
	__libc.auxv = auxv;
	__libc.page_size = __getauxval(AT_PAGESZ);
	if (!__libc.page_size) __libc.page_size = 4096;
	__libc.secure = __getauxval(AT_SECURE) != 0;

	/* dynamically linked: the dynamic linker set up the thread and TLS, and
	 * runs the constructors of every module (the program's last) */
	if (__libc.dl_init) {
		__libc.dl_init(argc, argv, envp);
		exit(main(argc, argv, envp));
	}

	/* Thread pointer, static TLS, canary: nothing above this line may
	 * touch errno or any thread-local state. */
	__init_tls(auxv);

	n = (size_t)(__preinit_array_end - __preinit_array_start);
	for (i = 0; i < n; i++) __preinit_array_start[i](argc, argv, envp);
	_init();
	n = (size_t)(__init_array_end - __init_array_start);
	for (i = 0; i < n; i++) __init_array_start[i](argc, argv, envp);

	exit(main(argc, argv, envp));
}
