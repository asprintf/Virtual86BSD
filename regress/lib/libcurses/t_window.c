/* windows: newwin, box (no acsc: the default characters), subwin, derwin,
 * mvwin, overlapping windows, wnoutrefresh and doupdate, delwin
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	WINDOW *w, *s, *d, *o;
	int y, x;

	initscr();
	refresh();
	w = newwin(5, 10, 1, 2);
	lg("newwin %s", w != NULL ? "ok" : "NULL");
	lg("box %s", rv(box(w, 0, 0)));
	mvwaddstr(w, 1, 1, "win");
	wrefresh(w);
	s = subwin(w, 2, 5, 3, 4);
	lg("subwin %s", s != NULL ? "ok" : "NULL");
	getbegyx(s, y, x);
	lg("subwin getbegyx %d %d", y, x);
	getparyx(s, y, x);
	lg("subwin getparyx %d %d", y, x);
	mvwaddstr(s, 0, 0, "sub");
	touchwin(w);
	wrefresh(w);
	d = derwin(w, 1, 3, 3, 6);
	getbegyx(d, y, x);
	lg("derwin getbegyx %d %d", y, x);
	getmaxyx(d, y, x);
	lg("derwin getmaxyx %d %d", y, x);
	lg("subwin too big %s", subwin(w, 9, 9, 0, 0) == NULL ? "NULL" : "?");
	o = newwin(2, 6, 4, 12);
	waddstr(o, "over");
	wnoutrefresh(w);
	wnoutrefresh(o);
	lg("doupdate %s", rv(doupdate()));
	lg("mvwin %s", rv(mvwin(o, 0, 14)));
	lg("mvwin off the screen %s", rv(mvwin(o, 7, 0)));
	touchwin(stdscr);
	wnoutrefresh(stdscr);
	touchwin(w);
	wnoutrefresh(w);
	wnoutrefresh(o);
	doupdate();
	lg("delwin %s %s", rv(delwin(d)), rv(delwin(s)));
	delwin(o);
	delwin(w);
	endwin();
	return 0;
}
