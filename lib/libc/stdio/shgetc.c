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

/* Reading with a limit (scanf field widths) and counting what was read. */
#include "stdio_impl.h"

void __shlim(FILE *f, off_t lim)
{
	f->shlim = lim;
	f->shcnt = f->buf - f->rpos;
	if (lim && f->rend - f->rpos > lim) f->shend = f->rpos + lim;
	else f->shend = f->rend;
}

int __shgetc(FILE *f)
{
	int c;
	off_t cnt = shcnt(f);
	if ((f->shlim && cnt >= f->shlim) || (c = __uflow(f)) < 0) {
		f->shcnt = f->buf - f->rpos + cnt;
		f->shend = f->rpos;
		f->shlim = -1;
		return EOF;
	}
	cnt++;
	if (f->shlim && f->rend - f->rpos > f->shlim - cnt) f->shend = f->rpos + (f->shlim - cnt);
	else f->shend = f->rend;
	f->shcnt = f->buf - f->rpos + cnt;
	if (f->rpos <= f->buf) f->rpos[-1] = (unsigned char)c;
	return c;
}
