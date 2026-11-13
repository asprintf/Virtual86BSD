/* getch: cbreak, noecho, keypad and function keys, nodelay, ungetch
 * OPS: e:ready s:a s:\eOA s:\eOD s:\eOP s:\x08 s:\r s:b t:300 w: */
#include "ct.h"

int
main(void)
{
	int c, i;

	initscr();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	mvaddstr(0, 0, "ready");
	refresh();
	for (i = 0; i < 7; i++) {
		c = getch();
		lg("getch %d %s", c, keyname(c));
	}
	lg("KEY_UP %d KEY_LEFT %d KEY_F(1) %d KEY_BACKSPACE %d", c == 'b',
	    KEY_LEFT > KEY_MIN, KEY_F(1) == KEY_F0 + 1, KEY_BACKSPACE >= KEY_MIN);
	nodelay(stdscr, TRUE);
	lg("nodelay: getch %s", getch() == ERR ? "ERR" : "?");
	lg("ungetch %s", rv(ungetch('z')));
	lg("getch after ungetch %c", getch());
	nodelay(stdscr, FALSE);
	mvaddstr(1, 0, "done");
	refresh();
	endwin();
	return 0;
}
