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

/* re_comp, re_exec: SUSv2; the matcher is sre.c */
#define _DEFAULT_SOURCE
#include <re_comp.h>
#include <stdlib.h>
#include <string.h>

int __sre_compile(const char *, size_t, unsigned char *, size_t, int);
int __sre_step(const char *, const unsigned char *, const char *, const char **, const char **);

static unsigned char *prog;

static char *msg(int e)
{
	switch (e) {
	case 11: return "Range endpoint too large";
	case 16: return "Bad number";
	case 25: return "\\digit out of range";
	case 36: return "Illegal or missing delimiter";
	case 41: return "No previous regular expression";
	case 42: return "\\( \\) imbalance";
	case 43: return "Too many \\(";
	case 44: return "More than two numbers given in \\{ \\}";
	case 45: return "} expected after \\";
	case 46: return "First number exceeds second in \\{ \\}";
	case 49: return "[ ] imbalance";
	default: return "Regular expression overflow";
	}
}

char *re_comp(const char *string)
{
	size_t n, size;
	unsigned char *p;
	int r;
	if (!string)
		return 0;
	if (!*string)
		return prog ? 0 : msg(41);
	n = strlen(string);
	size = 5 + 33 * n + 1;
	if (!(p = malloc(size)))
		return msg(50);
	if ((r = __sre_compile(string, n, p, size, 2)) < 0) {
		free(p);
		return msg(-r);
	}
	free(prog);
	prog = p;
	return 0;
}

int re_exec(const char *string)
{
	const char *s, *e;
	if (!prog)
		return -1;
	return __sre_step(string, prog, 0, &s, &e);
}
