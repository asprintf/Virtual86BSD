/* erasing and inserting: clrtoeol, clrtobot, deleteln, insertln, delch,
 * insch, insstr, erase
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	int i;

	initscr();
	for (i = 0; i < 8; i++)
		mvprintw(i, 0, "%d abcdefghij", i);
	move(0, 5);
	lg("clrtoeol %s", rv(clrtoeol()));
	move(1, 2);
	lg("delch %s", rv(delch()));
	move(2, 2);
	lg("insch %s", rv(insch('X')));
	move(3, 2);
	lg("insstr %s", rv(insstr("NEW")));
	move(4, 0);
	lg("deleteln %s", rv(deleteln()));
	move(5, 0);
	lg("insertln %s", rv(insertln()));
	mvaddstr(5, 0, "inserted");
	move(6, 4);
	lg("clrtobot %s", rv(clrtobot()));
	refresh();
	endwin();
	return 0;
}
