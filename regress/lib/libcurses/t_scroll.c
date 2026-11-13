/* scrolling: scrollok, the last line, scrl, setscrreg
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	int i;

	initscr();
	for (i = 0; i < 8; i++)
		mvprintw(i, 0, "line %d", i);
	refresh();
	lg("scroll without scrollok %s", rv(scroll(stdscr)));
	scrollok(stdscr, TRUE);
	lg("scroll %s", rv(scroll(stdscr)));
	mvaddstr(7, 0, "new 8");
	lg("scrl -1 %s", rv(scrl(-1)));
	move(7, 0);
	addstr("x\n");
	lg("setscrreg %s", rv(setscrreg(2, 4)));
	lg("setscrreg out of range %s", rv(setscrreg(4, 9)));
	lg("scrl 1 in the region %s", rv(scrl(1)));
	refresh();
	endwin();
	return 0;
}
