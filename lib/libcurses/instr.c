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

/* instr, innstr and their mv/w forms: X/Open Curses Issue 4 Version 2 */

#include <limits.h>
#include <string.h>
#include <wchar.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

int
instr(char *str)
{
	return winstr(stdscr, str);
}

int
innstr(char *str, int n)
{
	return winnstr(stdscr, str, n);
}

int
mvinstr(int y, int x, char *str)
{
	return mvwinstr(stdscr, y, x, str);
}

int
mvinnstr(int y, int x, char *str, int n)
{
	return mvwinnstr(stdscr, y, x, str, n);
}

int
mvwinstr(WINDOW *win, int y, int x, char *str)
{
	if (wmove(win, y, x) == ERR)
		return ERR;
	return winstr(win, str);
}

int
mvwinnstr(WINDOW *win, int y, int x, char *str, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;
	return winnstr(win, str, n);
}

#endif

int
winstr(WINDOW *win, char *str)
{
	return winnstr(win, str, -1) == ERR ? ERR : OK;
}

/* n < 0: no limit; otherwise n bytes at most, the terminating null included */
int
winnstr(WINDOW *win, char *str, int n)
{
	__LDATA *lp, *end;
	char mb[MB_LEN_MAX];
	size_t len, used;
	int cnt, step;

	if (__predict_false(win == NULL) || str == NULL || n == 0)
		return ERR;
	lp = &win->alines[win->cury]->line[win->curx];
	end = &win->alines[win->cury]->line[win->maxx];
	used = 0;
	cnt = 0;
	while (lp < end) {
		step = 1;
#ifdef HAVE_WCHAR
		if (lp->cflags & CA_CONTINUATION) {
			lp++;
			continue;
		}
		{
			mbstate_t st;

			memset(&st, 0, sizeof(st));
			len = wcrtomb(mb, lp->ch, &st);
			if (len == (size_t)-1)
				return ERR;
		}
		if (lp->wcols > 1)
			step = lp->wcols;
#else
		mb[0] = (char)(lp->ch & __CHARTEXT);
		len = 1;
#endif
		/* only whole characters, and room for the null */
		if (n > 0 && used + len >= (size_t)n)
			break;
		memcpy(str + used, mb, len);
		used += len;
		cnt++;
		lp += step;
	}
	str[used] = '\0';
	if (cnt == 0)
		return ERR;
	return n < 0 ? OK : cnt;
}
