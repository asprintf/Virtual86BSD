/*
 * Copyright (c) 2026, asprintf
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

/* makecontext: SUSv2, SUSv3 (getcontext and the rest: arch/x86_64/ucontext.s) */
/* Arguments are read as long, as glibc does, so pointers work too; func
 * returns to __start_context, which resumes uc_link (in %rbx). */
#define _DEFAULT_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <ucontext.h>

void __start_context(void);

enum { R8, R9, R10, R11, R12, R13, R14, R15, RDI, RSI, RBP, RBX, RDX, RAX, RCX, RSP, RIP };

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	static const int reg[6] = { RDI, RSI, RDX, RCX, R8, R9 };
	long long *g = ucp->uc_mcontext.__gregs;
	unsigned long *sp = (unsigned long *)((char *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	int i, nstack = argc > 6 ? argc - 6 : 0;
	va_list ap;
	sp -= nstack;
	sp = (unsigned long *)((uintptr_t)sp & ~(uintptr_t)15);
	va_start(ap, argc);
	for (i = 0; i < argc; i++) {
		long a = va_arg(ap, long);
		if (i < 6)
			g[reg[i]] = a;
		else
			sp[i - 6] = (unsigned long)a;
	}
	va_end(ap);
	*--sp = (unsigned long)__start_context;
	g[RSP] = (long long)(uintptr_t)sp;
	g[RIP] = (long long)(uintptr_t)func;
	g[RBX] = (long long)(uintptr_t)ucp->uc_link;
	g[RBP] = 0;
}
