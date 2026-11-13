/* colours: has_colors, start_color, COLORS, COLOR_PAIRS, init_pair,
 * pair_content, COLOR_PAIR, can_change_color
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	short f, b;

	initscr();
	lg("has_colors %d", has_colors());
	lg("start_color %s", rv(start_color()));
	lg("COLORS %d COLOR_PAIRS %d", COLORS, COLOR_PAIRS);
	lg("can_change_color %d", can_change_color());
	lg("init_pair %s", rv(init_pair(1, COLOR_RED, COLOR_BLUE)));
	lg("init_pair 2 %s", rv(init_pair(2, COLOR_GREEN, COLOR_BLACK)));
	pair_content(1, &f, &b);
	lg("pair_content %d %d", f, b);
	attron(COLOR_PAIR(1));
	mvaddstr(0, 0, "red on blue");
	attroff(COLOR_PAIR(1));
	addstr(" n");
	attron(COLOR_PAIR(2) | A_BOLD);
	mvaddstr(1, 0, "green");
	attrset(A_NORMAL);
	lg("PAIR_NUMBER %d", (int)PAIR_NUMBER(COLOR_PAIR(2) | A_BOLD));
	color_set(1, NULL);
	mvaddstr(2, 0, "set");
	color_set(0, NULL);
	refresh();
	endwin();
	return 0;
}
