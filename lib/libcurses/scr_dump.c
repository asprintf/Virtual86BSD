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

/* scr_dump, scr_restore, scr_init, scr_set: X/Open Curses Issue 4
 * Version 2; the file is a window as putwin() writes it */

#include <stdio.h>
#include <unistd.h>

#include "curses.h"
#include "curses_private.h"

static WINDOW *
readscr(const char *filename)
{
	FILE *fp;
	WINDOW *win;

	if (_cursesi_screen == NULL || (fp = fopen(filename, "r")) == NULL)
		return NULL;
	win = getwin(fp);
	fclose(fp);
	return win;
}

/* the window's contents into dst, the virtual screen or curscr; dst's
 * cursor stays where it was (overwrite() moves it, and leaves the last
 * line past its end) */
static int
readinto(const char *filename, WINDOW *dst)
{
	WINDOW *win;
	int r, y, x, i;

	if ((win = readscr(filename)) == NULL)
		return ERR;
	y = dst->cury;
	x = dst->curx;
	r = overwrite(win, dst);
	delwin(win);
	for (i = 0; i < dst->maxy; i++)
		dst->alines[i]->flags &= ~__ISPASTEOL;
	dst->cury = y;
	dst->curx = x;
	return r;
}

int
scr_dump(const char *filename)
{
	FILE *fp;
	int r;

	if (_cursesi_screen == NULL || (fp = fopen(filename, "w")) == NULL)
		return ERR;
	r = putwin(__virtscr, fp);
	if (fclose(fp) == EOF)
		r = ERR;
	return r;
}

int
scr_restore(const char *filename)
{

	return readinto(filename, __virtscr);
}

int
scr_init(const char *filename)
{

	/* with rmcup and nrrmc the screen is not what was dumped */
	if (cur_term != NULL && exit_ca_mode != NULL && non_rev_rmcup) {
		if (_cursesi_screen == NULL || access(filename, R_OK) != 0)
			return ERR;
		return OK;
	}
	return readinto(filename, curscr);
}

int
scr_set(const char *filename)
{

	if (scr_restore(filename) == ERR)
		return ERR;
	return scr_init(filename);
}
