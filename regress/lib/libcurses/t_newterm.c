/* newterm, set_term, delscreen: a screen on the terminal's streams
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	SCREEN *s, *o;

	s = newterm("v86test", stdout, stdin);
	lg("newterm %s", s != NULL ? "ok" : "NULL");
	lg("newterm nosuchterminal %s", newterm("nosuchterminal", stdout, stdin) == NULL ? "NULL" : "?");
	o = set_term(s);
	lg("set_term %s", o != NULL ? "ok" : "NULL");
	lg("LINES %d COLS %d", LINES, COLS);
	mvaddstr(0, 0, "newterm");
	refresh();
	endwin();
	delscreen(s);
	return 0;
}
