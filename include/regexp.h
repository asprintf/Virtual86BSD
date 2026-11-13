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

/* <regexp.h>: SUSv2; compile() uses the program's INIT, GETC() ... macros */
#ifndef _REGEXP_H
#define _REGEXP_H
#include <bits/features.h>

#if __XSI_OB5
__BEGIN_DECLS
extern char *loc1, *loc2, *locs;
int advance(const char *, const char *);
int step(const char *, const char *);
void __regexp_start(void);
void __regexp_add(int);
char *__regexp_end(char *, const char *, int *);
__END_DECLS

static __INLINE char *compile(char *instring, char *expbuf, const char *endbuf, int eof)
{
	INIT
	int __c, __e;
	(void) instring;
	__regexp_start();
	for (;;) {
		__c = GETC();
		if (__c == eof)
			break;
		if (__c == '\n' || __c == '\0')
			ERROR(36);
		__regexp_add(__c);
		if (__c == '\\') {
			__c = GETC();
			if (__c == '\n' || __c == '\0')
				ERROR(36);
			__regexp_add(__c);
		}
	}
	expbuf = __regexp_end(expbuf, endbuf, &__e);
	if (__e)
		ERROR(__e);
	RETURN(expbuf);
}
#endif
#endif
