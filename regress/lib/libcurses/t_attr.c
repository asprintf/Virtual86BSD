/* renditions: attron, attroff, attrset, standout, attr_on (WA_), chgat,
 * and what inch() and attr_get() read back
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	attr_t a;
	short p;
	chtype c;

	initscr();
	attron(A_BOLD);
	mvaddstr(0, 0, "bold");
	attroff(A_BOLD);
	addstr(" n");
	attron(A_UNDERLINE);
	mvaddstr(1, 0, "under");
	attrset(A_NORMAL);
	attron(A_REVERSE);
	mvaddstr(2, 0, "rev");
	attrset(A_REVERSE | A_BOLD);
	addstr("rb");
	attrset(0);
	standout();
	mvaddstr(3, 0, "so");
	standend();
	addstr("n");
	attr_on(WA_DIM, NULL);
	mvaddstr(4, 0, "dim");
	attr_off(WA_DIM, NULL);
	attr_on(WA_BLINK, NULL);
	addstr("blink");
	attr_set(WA_INVIS, 0, NULL);
	addstr("inv");
	attr_set(WA_NORMAL, 0, NULL);
	mvaddstr(5, 0, "chgat here");
	lg("mvchgat %s", rv(mvchgat(5, 0, 5, A_BOLD, 0, NULL)));
	mvaddch(6, 0, 'x' | A_UNDERLINE);
	addch('y');
	c = mvinch(0, 0);
	lg("inch(0,0): char %c bold %d underline %d", (int)(c & A_CHARTEXT),
	    (c & A_BOLD) != 0, (c & A_UNDERLINE) != 0);
	c = mvinch(6, 0);
	lg("inch(6,0): char %c underline %d", (int)(c & A_CHARTEXT), (c & A_UNDERLINE) != 0);
	attrset(A_BOLD | A_UNDERLINE);
	attr_get(&a, &p, NULL);
	lg("attr_get: bold %d underline %d reverse %d pair %d", (a & WA_BOLD) != 0,
	    (a & WA_UNDERLINE) != 0, (a & WA_REVERSE) != 0, p);
	attrset(A_NORMAL);
	lg("termattrs: bold %d underline %d reverse %d blink %d dim %d standout %d",
	    (termattrs() & A_BOLD) != 0, (termattrs() & A_UNDERLINE) != 0,
	    (termattrs() & A_REVERSE) != 0, (termattrs() & A_BLINK) != 0,
	    (termattrs() & A_DIM) != 0, (termattrs() & A_STANDOUT) != 0);
	refresh();
	endwin();
	return 0;
}
