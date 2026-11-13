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

/* inchstr, inchnstr and their mv/w forms: X/Open Curses Issue 4 Version 2 */

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

int
inchstr(chtype *chstr)
{
	return winchstr(stdscr, chstr);
}

int
inchnstr(chtype *chstr, int n)
{
	return winchnstr(stdscr, chstr, n);
}

int
mvinchstr(int y, int x, chtype *chstr)
{
	return mvwinchstr(stdscr, y, x, chstr);
}

int
mvinchnstr(int y, int x, chtype *chstr, int n)
{
	return mvwinchnstr(stdscr, y, x, chstr, n);
}

int
mvwinchstr(WINDOW *win, int y, int x, chtype *chstr)
{
	if (wmove(win, y, x) == ERR)
		return ERR;
	return winchstr(win, chstr);
}

int
mvwinchnstr(WINDOW *win, int y, int x, chtype *chstr, int n)
{
	if (wmove(win, y, x) == ERR)
		return ERR;
	return winchnstr(win, chstr, n);
}

#endif

int
winchstr(WINDOW *win, chtype *chstr)
{
	return winchnstr(win, chstr, -1);
}

/* n < 0: no limit; otherwise n elements at most, the terminating 0 included */
int
winchnstr(WINDOW *win, chtype *chstr, int n)
{
	__LDATA *lp, *end;
	attr_t attr;
	int i;

	if (__predict_false(win == NULL) || chstr == NULL || n == 0)
		return ERR;
	lp = &win->alines[win->cury]->line[win->curx];
	end = &win->alines[win->cury]->line[win->maxx];
	for (i = 0; lp < end && (n < 0 || i < n - 1); lp++) {
#ifdef HAVE_WCHAR
		if (lp->cflags & CA_CONTINUATION)
			continue;
#endif
		attr = lp->attr & __ATTRIBUTES;
		/* as winch(): the default colour pair reads as no colour */
		if (__using_color && (attr & __COLOR) == __default_color)
			attr &= ~__COLOR;
		chstr[i++] = (chtype)(lp->ch & __CHARTEXT) | attr;
	}
	chstr[i] = 0;
	return OK;
}
