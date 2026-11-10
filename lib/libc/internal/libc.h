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

/* Internal: process-wide library state. */
#ifndef _INTERNAL_LIBC_H
#define _INTERNAL_LIBC_H
#include <stddef.h>

struct __pthread;

struct __tls_module {
	struct __tls_module *next;
	void *image;        /* initialisation image (.tdata) */
	size_t len;         /* bytes of image */
	size_t size;        /* total size (.tdata + .tbss) */
	size_t align;
	size_t offset;      /* distance from the thread pointer down to the block */
	size_t id;          /* its index in the dynamic thread vector (dtv) */
};

struct __libc {
	unsigned long *auxv;
	size_t page_size;
	int threaded;               /* set once a second thread was created */
	volatile int threads_minus_1;  /* the number of threads less one */
	int secure;                 /* AT_SECURE */
	struct __tls_module *tls_head;  /* the modules of the static TLS area */
	size_t tls_cnt;             /* how many (the dtv has one more entry) */
	size_t tls_area_size;       /* TLS + TCB, rounded, for new threads */
	size_t tls_align;
	/* set by the dynamic linker (libc.so): it ran the start-up, and runs
	 * the constructors and destructors of all modules */
	void (*dl_init)(int, char **, char **);
	void (*dl_fini)(void);
	void *(*tls_get_new)(unsigned long *);   /* a dlopen'ed module's TLS */
	void (*tls_free)(struct __pthread *);     /* at thread exit */
};

extern struct __libc __libc;
extern char **__environ;


void __init_tls(unsigned long *auxv);
struct __pthread *__copy_tls(unsigned char *mem);
void __init_tp(struct __pthread *);
void __tls_layout(void);
void __init_main_tls(void);
void __libc_exit_fini(void);
void __funcs_on_exit(void);
int __set_thread_area(void *);

#endif
