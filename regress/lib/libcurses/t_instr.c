/* reading the window back: instr, innstr, inchstr, inchnstr, winch
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	char buf[64];
	chtype chs[64];
	int r, i;

	initscr();
	mvaddstr(0, 0, "hello world");
	attron(A_BOLD);
	mvaddstr(1, 2, "BOLD");
	attroff(A_BOLD);
	r = mvinstr(0, 0, buf);
	lg("mvinstr %s [%s]", rv(r), buf);
	r = mvinstr(0, 15, buf);
	lg("mvinstr at 15 %s [%s]", rv(r), buf);
	r = mvinnstr(0, 6, buf, 4);
	lg("mvinnstr 4 %d [%s]", r, buf);
	r = mvinnstr(0, 6, buf, 1);
	lg("mvinnstr 1 %s", rv(r));
	r = mvinnstr(0, 0, buf, 64);
	lg("mvinnstr 64 %d [%s]", r, buf);
	r = mvinstr(8, 0, buf);
	lg("mvinstr below the screen %s", rv(r));
	r = mvinchstr(1, 0, chs);
	for (i = 0; chs[i] != 0; i++)
		;
	lg("mvinchstr %s, %d cells; [2] %c bold %d, [6] %c bold %d", rv(r), i,
	    (int)(chs[2] & A_CHARTEXT), (chs[2] & A_BOLD) != 0,
	    (int)(chs[6] & A_CHARTEXT), (chs[6] & A_BOLD) != 0);
	r = mvinchnstr(1, 2, chs, 3);
	for (i = 0; chs[i] != 0; i++)
		;
	lg("mvinchnstr 3 %s, %d cells", rv(r), i);
	move(1, 3);
	lg("winch %c bold %d", (int)(winch(stdscr) & A_CHARTEXT), (winch(stdscr) & A_BOLD) != 0);
	refresh();
	endwin();
	return 0;
}
