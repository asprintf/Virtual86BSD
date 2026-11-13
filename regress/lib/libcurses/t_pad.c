/* pads: newpad, prefresh, pnoutrefresh, subpad
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	WINDOW *p, *s;
	int i;

	initscr();
	refresh();
	p = newpad(20, 30);
	lg("newpad %s", p != NULL ? "ok" : "NULL");
	for (i = 0; i < 20; i++)
		mvwprintw(p, i, 0, "pad line %d ......", i);
	lg("prefresh %s", rv(prefresh(p, 5, 4, 1, 2, 3, 15)));
	s = subpad(p, 2, 6, 10, 0);
	lg("subpad %s", s != NULL ? "ok" : "NULL");
	/* (written through the pad: the subpad is touched before its refresh) */
	touchwin(s);
	lg("pnoutrefresh %s", rv(pnoutrefresh(s, 0, 0, 5, 0, 6, 5)));
	doupdate();
	lg("prefresh outside the screen %s", rv(prefresh(p, 0, 0, 6, 0, 9, 10)));
	endwin();
	return 0;
}
