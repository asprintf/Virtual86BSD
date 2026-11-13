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

/* regcmp, regex, __loc1: SUSv2; translated to an extended RE for regcomp */
/* The compiled form, one malloc()ed block: "RCMP", the group of each $0..$9,
 * the extended RE. */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <libgen.h>
#include <regex.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char *__loc1;

#define TAG 4
#define MAP 10

char *regcmp(const char *string1, ...)
{
	va_list ap;
	const char *s;
	size_t len = 0, o = TAG + MAP;
	char *pat, *r;
	int group = 0, stack[64], depth = 0;
	regex_t re;
	va_start(ap, string1);
	for (s = string1; s; s = va_arg(ap, const char *))
		len += strlen(s);
	va_end(ap);
	if (!(pat = malloc(len + 1)) || !(r = malloc(TAG + MAP + 2 * len + 1))) {
		free(pat);
		errno = ENOMEM;
		return 0;
	}
	*pat = 0;
	va_start(ap, string1);
	for (s = string1, len = 0; s; s = va_arg(ap, const char *)) {
		memcpy(pat + len, s, strlen(s) + 1);
		len += strlen(s);
	}
	va_end(ap);
	memcpy(r, "RCMP", TAG);
	memset(r + TAG, 0, MAP);
	for (s = pat; *s; s++) {
		switch (*s) {
		case '\\':
			if (!s[1])
				goto bad;
			s++;
			if (*s == 'n')
				r[o++] = '\n';
			else if (strchr("^.[$()|*+?{\\", *s)) {
				r[o++] = '\\';
				r[o++] = *s;
			} else
				r[o++] = *s;
			continue;
		case '[':
			r[o++] = *s++;
			if (*s == '^')
				r[o++] = *s++;
			if (*s == ']')
				r[o++] = *s++;
			while (*s && *s != ']')
				r[o++] = *s++;
			if (!*s)
				goto bad;
			r[o++] = ']';
			continue;
		case '(':
			if (depth == 64)
				goto bad;
			stack[depth++] = ++group;
			r[o++] = '(';
			continue;
		case ')':
			if (!depth)
				goto bad;
			r[o++] = ')';
			depth--;
			if (s[1] == '$' && s[2] >= '0' && s[2] <= '9') {
				r[TAG + (s[2] - '0')] = (char)stack[depth];
				s += 2;
			}
			continue;
		case '|': case '?':
			/* ordinary here, special in an extended expression */
			r[o++] = '\\';
			r[o++] = *s;
			continue;
		default:
			r[o++] = *s;
		}
	}
	r[o] = 0;
	free(pat);
	if (depth || group > 255 || regcomp(&re, r + TAG + MAP, REG_EXTENDED)) {
		free(r);
		return 0;
	}
	regfree(&re);
	return r;
bad:
	free(pat);
	free(r);
	return 0;
}

char *regex(const char *re, const char *subject, ...)
{
	regex_t r;
	regmatch_t m[256];
	va_list ap;
	char *ret[MAP];
	int n, last = -1;
	if (!re || memcmp(re, "RCMP", TAG) ||
	    regcomp(&r, re + TAG + MAP, REG_EXTENDED))
		return 0;
	n = regexec(&r, subject, r.re_nsub + 1 < 256 ? r.re_nsub + 1 : 256, m, 0);
	regfree(&r);
	if (n)
		return 0;
	for (n = 0; n < MAP; n++)
		if (re[TAG + n])
			last = n;
	va_start(ap, subject);
	for (n = 0; n <= last; n++)
		ret[n] = va_arg(ap, char *);
	va_end(ap);
	for (n = 0; n <= last; n++) {
		int g = (unsigned char)re[TAG + n];
		if (!g)
			continue;
		if (m[g].rm_so < 0)
			ret[n][0] = 0;
		else {
			memcpy(ret[n], subject + m[g].rm_so, (size_t)(m[g].rm_eo - m[g].rm_so));
			ret[n][m[g].rm_eo - m[g].rm_so] = 0;
		}
	}
	__loc1 = (char *)subject + m[0].rm_so;
	return (char *)subject + m[0].rm_eo;
}
