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

#include <wchar.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* through strftime(): the format to multibyte, the result back */
size_t wcsftime(wchar_t *__RESTRICT s, size_t max, const wchar_t *__RESTRICT fmt, const struct tm *__RESTRICT tm)
{
	const wchar_t *p = fmt;
	char *f, *b;
	size_t fl = wcsrtombs(0, &p, 0, 0), bl, r, n;
	const char *q;
	if (fl == (size_t)-1 || !max) return 0;
	if (!(f = malloc(fl + 1))) return 0;
	p = fmt;
	wcsrtombs(f, &p, fl + 1, 0);
	bl = max * 4 + fl * 4 + 256;
	if (!(b = malloc(bl))) { free(f); return 0; }
	r = strftime(b, bl, f, tm);
	free(f);
	if (!r && *fmt) { free(b); return 0; }
	q = b;
	n = mbsrtowcs(s, &q, max, 0);
	free(b);
	if (n == (size_t)-1 || n >= max) return 0;
	return n;
}
