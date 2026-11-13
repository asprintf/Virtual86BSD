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

/* vtscreen - the screen a terminal of type v86test (v86test.ti) shows
 * after the output read from the standard input: its lines, and below a
 * line with renditions or colours a line of their codes.
 *   vtscreen LINES COLUMNS < output
 * Each rendition is a bit: 1 bold, 2 dim, 4 underline, 8 blink, 16
 * reverse, 32 invisible; the code of a cell is ".", or the sum as a
 * character of 0-9a-zA-Z+/.  Colours: the foreground then the background
 * number of each cell, or "." for the default, which white on black is
 * (X/Open Curses assumes a black background).  A sequence the terminal
 * does not know is reported ("unknown: ..."), and so are bells. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXL 50
#define MAXC 200

struct cell { char ch; int attr, fg, bg; };
static struct cell scr[MAXL][MAXC];
static int lines, cols, y, x, attr, fg = -1, bg = -1, bells;

static void blank(struct cell *c)
{
	c->ch = ' ';
	c->attr = 0;
	c->fg = c->bg = -1;
}

static void clearfrom(int ly, int lx)
{
	for (; ly < lines; ly++, lx = 0)
		for (; lx < cols; lx++)
			blank(&scr[ly][lx]);
}

static void down(void)
{
	if (y < lines - 1) {
		y++;
		return;
	}
	memmove(scr[0], scr[1], sizeof scr[0] * (size_t)(lines - 1));
	for (int i = 0; i < cols; i++)
		blank(&scr[lines - 1][i]);
}

static void sgr(int *p, int np)
{
	if (np == 0) {
		attr = 0;
		fg = bg = -1;
	}
	for (int i = 0; i < np; i++) {
		int v = p[i];
		if (v == 0) { attr = 0; fg = bg = -1; }
		else if (v == 1) attr |= 1;
		else if (v == 2) attr |= 2;
		else if (v == 4) attr |= 4;
		else if (v == 5) attr |= 8;
		else if (v == 7) attr |= 16;
		else if (v == 8) attr |= 32;
		else if (v >= 30 && v <= 37) fg = v == 37 ? -1 : v - 30;
		else if (v == 39) fg = -1;
		else if (v >= 40 && v <= 47) bg = v == 40 ? -1 : v - 40;
		else if (v == 49) bg = -1;
		else printf("unknown: \\E[%dm\n", v);
	}
}

int main(int argc, char **argv)
{
	int c;

	if (argc != 3 || (lines = atoi(argv[1])) < 1 || lines > MAXL ||
	    (cols = atoi(argv[2])) < 1 || cols > MAXC) {
		fprintf(stderr, "usage: vtscreen lines columns < output\n");
		return 2;
	}
	clearfrom(0, 0);
	while ((c = getchar()) != EOF) {
		if (c == 033) {
			int p[16], np = 0, v = -1, f;
			if ((c = getchar()) != '[') {
				printf("unknown: \\E%c\n", c == EOF ? '?' : c);
				continue;
			}
			while ((f = getchar()) != EOF && ((f >= '0' && f <= '9') || f == ';')) {
				if (f == ';') {
					if (np < 16) p[np++] = v < 0 ? 0 : v;
					v = -1;
				} else
					v = (v < 0 ? 0 : v * 10) + (f - '0');
			}
			if (v >= 0 && np < 16)
				p[np++] = v;
			switch (f) {
			case 'H':
				y = (np > 0 && p[0] > 0 ? p[0] : 1) - 1;
				x = (np > 1 && p[1] > 0 ? p[1] : 1) - 1;
				if (y >= lines) y = lines - 1;
				if (x >= cols) x = cols - 1;
				break;
			case 'J':
				if (np == 0 || p[0] == 0) clearfrom(y, x);
				else if (p[0] == 2) clearfrom(0, 0);
				else printf("unknown: \\E[%dJ\n", p[0]);
				break;
			case 'K':
				for (int i = x; i < cols; i++)
					blank(&scr[y][i]);
				break;
			case 'm':
				sgr(p, np);
				break;
			case 'A': y -= np ? p[0] : 1; if (y < 0) y = 0; break;
			case 'B': y += np ? p[0] : 1; if (y >= lines) y = lines - 1; break;
			case 'C': x += np ? p[0] : 1; if (x >= cols) x = cols - 1; break;
			case 'D': x -= np ? p[0] : 1; if (x < 0) x = 0; break;
			default:
				printf("unknown: \\E[...%c\n", f == EOF ? '?' : f);
			}
		} else if (c == '\r')
			x = 0;
		else if (c == '\n')
			down();
		else if (c == '\b') {
			if (x > 0) x--;
		} else if (c == '\t') {
			x = (x / 8 + 1) * 8;
			if (x >= cols) x = cols - 1;
		} else if (c == '\a')
			bells++;
		else if (c >= ' ' && c < 0177) {
			/* no automatic margins: the last column is written over */
			struct cell *e = &scr[y][x];
			e->ch = (char)c;
			e->attr = attr;
			e->fg = fg;
			e->bg = bg;
			if (x < cols - 1)
				x++;
		} else
			printf("unknown: \\%03o\n", c);
	}
	if (bells)
		printf("bells: %d\n", bells);
	for (int i = 0; i < lines; i++) {
		int a = 0, k = 0;
		putchar('|');
		for (int j = 0; j < cols; j++) {
			putchar(scr[i][j].ch);
			a |= scr[i][j].attr;
			k |= scr[i][j].fg >= 0 || scr[i][j].bg >= 0;
		}
		printf("|\n");
		if (a) {
			putchar(' ');
			for (int j = 0; j < cols; j++)
				putchar(scr[i][j].attr ? "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/"[scr[i][j].attr] : '.');
			putchar('\n');
		}
		if (k) {
			putchar(' ');
			for (int j = 0; j < cols; j++)
				putchar(scr[i][j].fg >= 0 ? '0' + scr[i][j].fg : '.');
			printf("\n ");
			for (int j = 0; j < cols; j++)
				putchar(scr[i][j].bg >= 0 ? '0' + scr[i][j].bg : '.');
			putchar('\n');
		}
	}
	return 0;
}
