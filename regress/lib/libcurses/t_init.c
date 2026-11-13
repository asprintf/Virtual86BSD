/* initscr, the screen's size, addstr, mvaddstr, getyx, refresh, endwin
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	WINDOW *w;
	int y, x;

	w = initscr();
	lg("initscr: %s", w == stdscr ? "stdscr" : "?");
	lg("LINES %d COLS %d", LINES, COLS);
	lg("curscr %s", curscr != NULL ? "set" : "NULL");
	lg("addstr %s", rv(addstr("hello")));
	getyx(stdscr, y, x);
	lg("getyx %d %d", y, x);
	lg("mvaddstr %s", rv(mvaddstr(2, 3, "world")));
	lg("mvaddstr below the screen %s", rv(mvaddstr(8, 0, "x")));
	lg("mvaddstr at the last line %s", rv(mvaddstr(7, 0, "last")));
	getmaxyx(stdscr, y, x);
	lg("getmaxyx %d %d", y, x);
	getbegyx(stdscr, y, x);
	lg("getbegyx %d %d", y, x);
	lg("refresh %s", rv(refresh()));
	lg("isendwin %d", isendwin());
	lg("endwin %s", rv(endwin()));
	lg("isendwin %d", isendwin());
	return 0;
}
